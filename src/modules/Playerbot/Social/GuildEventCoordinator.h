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
#include "Group.h"
#include "CalendarMgr.h"
#include "../Core/DI/Interfaces/IGuildEventCoordinator.h"
#include <unordered_map>
#include <vector>
#include <queue>
#include <atomic>
#include <mutex>
#include <chrono>

namespace Playerbot
{

// Enums and structs defined in IGuildEventCoordinator.h interface:
// - enum class GuildEventType
// - enum class EventStatus
// - struct EventCoordinationProfile

enum class EventPriority : uint8
{
    LOW             = 0,
    NORMAL          = 1,
    HIGH            = 2,
    CRITICAL        = 3
};

struct GuildEvent
{
    uint32 eventId;
    uint32 guildId;
    uint32 organizerGuid;
    std::string organizerName;
    std::string eventTitle;
    std::string eventDescription;
    GuildEventType eventType;
    EventStatus status;
    EventPriority priority;
    uint32 scheduledTime;
    uint32 duration;
    uint32 maxParticipants;
    uint32 minParticipants;
    std::vector<uint32> invitedMembers;
    std::vector<uint32> confirmedMembers;
    std::vector<uint32> declinedMembers;
    std::unordered_map<uint32, std::string> memberRoles; // memberGuid -> role
    uint32 creationTime;
    uint32 lastUpdateTime;
    bool isRecurring;
    uint32 recurringInterval;

    GuildEvent() : eventId(0), guildId(0), organizerGuid(0)
        , eventType(GuildEventType::SOCIAL_GATHERING), status(EventStatus::PLANNING)
        , priority(EventPriority::NORMAL), scheduledTime(0), duration(7200000) // 2 hours
        , maxParticipants(40), minParticipants(5), creationTime(GameTime::GetGameTimeMS())
        , lastUpdateTime(GameTime::GetGameTimeMS()), isRecurring(false), recurringInterval(0) {}
};

/**
 * @brief Advanced guild event coordination system for automated event management
 *
 * This system provides intelligent guild event planning, scheduling, coordination,
 * and management for playerbots using TrinityCore's calendar and guild systems.
 */
class TC_GAME_API GuildEventCoordinator final : public IGuildEventCoordinator
{
public:
    explicit GuildEventCoordinator(Player* bot);
    ~GuildEventCoordinator();
    GuildEventCoordinator(GuildEventCoordinator const&) = delete;
    GuildEventCoordinator& operator=(GuildEventCoordinator const&) = delete;

    // Core event management using TrinityCore's Calendar system
    uint32 CreateGuildEvent(Player* organizer, const GuildEvent& eventData) override;
    bool UpdateGuildEvent(uint32 eventId, const GuildEvent& updatedData) override;
    bool CancelGuildEvent(Player* organizer, uint32 eventId) override;
    void ProcessEventInvitations(uint32 eventId) override;

    // Event planning and scheduling
    void PlanGuildEvents() override;
    void ScheduleRecurringEvents() override;
    void ProposeEventIdeas();
    void CoordinateEventTiming(uint32 eventId);

    // Event recruitment and coordination
    void RecruitEventParticipants(Player* organizer, uint32 eventId) override;
    void ManageEventSignups(uint32 eventId) override;
    void AssignEventRoles(uint32 eventId) override;
    void HandleEventChanges(uint32 eventId);

    // Event execution and management
    void ExecuteGuildEvent(uint32 eventId) override;
    void CoordinateEventActivities(Player* leader, uint32 eventId) override;
    void MonitorEventProgress(uint32 eventId) override;
    void HandleEventCompletion(uint32 eventId) override;

    // Advanced event features (EventCoordinationProfile struct defined in IGuildEventCoordinator.h)
    void SetEventProfile(const EventCoordinationProfile& profile) override;
    EventCoordinationProfile GetEventProfile() override;

    // Event analytics and tracking (EventParticipation struct defined in IGuildEventCoordinator.h)
    EventParticipation GetEventParticipation() override;
    void UpdateEventParticipation(uint32 eventId, bool wasOrganizer);

    // Event type specific coordination
    void CoordinateRaidEvent(Player* leader, uint32 eventId);
    void CoordinatePvPEvent(Player* leader, uint32 eventId);
    void CoordinateSocialEvent(Player* leader, uint32 eventId);
    void CoordinateLevelingEvent(Player* leader, uint32 eventId);

    // Event optimization and intelligence
    void OptimizeEventScheduling() override;
    void AnalyzeGuildEventPatterns(uint32 guildId) override;
    void SuggestOptimalEventTimes(uint32 guildId);
    std::vector<GuildEventType> RecommendEventTypes();

    // Event communication and updates
    void BroadcastEventUpdates(uint32 eventId, const std::string& updateMessage) override;
    void SendEventReminders(uint32 eventId) override;
    void NotifyEventChanges(uint32 eventId);
    void UpdateEventStatus(uint32 eventId, EventStatus newStatus) override;

    // Group formation for events
    Group* FormEventGroup(uint32 eventId) override;
    void AssignGroupRoles(Group* group, uint32 eventId) override;
    void CoordinateGroupForEvent(Group* group, uint32 eventId);
    void HandleEventGroupChanges(Group* group, uint32 eventId);

    // Event conflict resolution
    void ResolveEventConflicts();
    void HandleOverlappingEvents(uint32 eventId1, uint32 eventId2);
    void ManageEventPriorities(uint32 guildId);
    void RescheduleConflictingEvents();

    // Performance monitoring (EventMetrics struct defined in IGuildEventCoordinator.h)
    EventMetrics GetGuildEventMetrics(uint32 guildId) override;
    EventMetrics GetPlayerEventMetrics() override;

    // Event templates and presets
    void CreateEventTemplate(const std::string& templateName, const GuildEvent& templateData);
    GuildEvent LoadEventTemplate(const std::string& templateName);
    std::vector<std::string> GetAvailableTemplates();
    void CustomizeEventFromTemplate(GuildEvent& event, const std::string& templateName);

    // Seasonal and special events
    void PlanSeasonalEvents(uint32 guildId);
    void HandleHolidayEvents(uint32 guildId);
    void OrganizeSpecialCelebrations(uint32 guildId);
    void CoordinateGuildAnniversary(uint32 guildId);

    // Configuration and customization
    void SetEventCoordinationEnabled(uint32 guildId, bool enabled) override;
    void SetMaxConcurrentEvents(uint32 guildId, uint32 maxEvents);
    void ConfigureEventNotifications(bool enableReminders);
    void SetEventAutoSignup(GuildEventType eventType, bool autoSignup);

    // Error handling and recovery
    void HandleEventError(uint32 eventId, const std::string& error);
    void RecoverFromEventFailure(uint32 eventId);
    void HandleMissingOrganizer(uint32 eventId);
    void EmergencyEventCancellation(uint32 eventId);

    // Update and maintenance
    void Update(uint32 diff) override;
    void UpdateEventStates() override;
    void ProcessEventReminders();
    void CleanupExpiredEvents() override;

private:
    Player* _bot;

    // Core event data
    std::unordered_map<uint32, GuildEvent> _guildEvents; // eventId -> event
    std::unordered_map<uint32, EventCoordinationProfile> _playerProfiles; // playerGuid -> profile
    std::unordered_map<uint32, EventParticipation> _playerParticipation; // playerGuid -> participation
    std::atomic<uint32> _nextEventId{1};
    

    // Guild event tracking
    std::unordered_map<uint32, std::vector<uint32>> _guildActiveEvents; // guildId -> eventIds
    std::unordered_map<uint32, EventMetrics> _guildMetrics; // guildId -> metrics

    // Event templates
    std::unordered_map<std::string, GuildEvent> _eventTemplates; // templateName -> template
    std::unordered_map<GuildEventType, std::vector<std::string>> _typeTemplates; // eventType -> templateNames

    // Event scheduling intelligence
    struct GuildEventScheduler
    {
        uint32 guildId;
        std::vector<std::pair<uint32, uint32>> popularTimes; // startTime, endTime
        std::unordered_map<uint32, float> memberAvailability; // memberGuid -> availability score
        std::vector<uint32> conflictingEvents;
        uint32 optimalEventDuration;
        float averageAttendanceRate;

        GuildEventScheduler() : guildId(0), optimalEventDuration(7200000), averageAttendanceRate(0.75f) {}

        GuildEventScheduler(uint32 gId) : guildId(gId), optimalEventDuration(7200000)
            , averageAttendanceRate(0.75f) {}
    };

    std::unordered_map<uint32, GuildEventScheduler> _guildSchedulers; // guildId -> scheduler

    // Performance tracking
    EventMetrics _globalMetrics;

    // Helper functions
    void InitializeEventTemplates();
    void LoadGuildEventData(uint32 guildId);
    bool ValidateEventData(const GuildEvent& event);
    void UpdateEventCalendar(uint32 eventId);

    // Event planning algorithms
    uint32 FindOptimalEventTime(uint32 guildId, GuildEventType eventType, uint32 duration);
    std::vector<uint32> SelectEventParticipants(uint32 eventId);
    void OptimizeEventComposition(uint32 eventId);
    bool IsEventViable(const GuildEvent& event);

    // Event coordination implementations
    void CoordinateEventPreparation(uint32 eventId);
    void ManageEventExecution(uint32 eventId);
    void HandleEventLogistics(uint32 eventId);
    void MonitorEventHealth(uint32 eventId);

    // Communication helpers
    void SendEventInvitation(uint32 eventId);
    void BroadcastEventAnnouncement(uint32 eventId);
    void UpdateEventParticipants(uint32 eventId, const std::string& message);
    void LogEventActivity(uint32 eventId, const std::string& activity);

    // Performance optimization
    void OptimizeEventScheduling(uint32 guildId);
    void CacheEventData(uint32 guildId);
    void PreloadEventInformation(uint32 eventId);
    void UpdateEventMetrics(uint32 eventId, bool wasSuccessful);

    // Constants
    static constexpr uint32 EVENT_UPDATE_INTERVAL = 60000; // 1 minute
    static constexpr uint32 REMINDER_CHECK_INTERVAL = 300000; // 5 minutes
    static constexpr uint32 PLANNING_ADVANCE_TIME = 604800000; // 1 week
    static constexpr uint32 MAX_EVENTS_PER_GUILD = 20;
    static constexpr uint32 MIN_EVENT_DURATION = 1800000; // 30 minutes
    static constexpr uint32 MAX_EVENT_DURATION = 14400000; // 4 hours
    static constexpr float MIN_ATTENDANCE_RATE = 0.4f; // 40% minimum for viability
    static constexpr uint32 EVENT_REMINDER_TIMES[3] = {86400000, 3600000, 1800000}; // 24h, 1h, 30m
    static constexpr uint32 MAX_RECURRING_EVENTS = 5;
    static constexpr uint32 EVENT_HISTORY_RETENTION = 2592000000; // 30 days
};

} // namespace Playerbot