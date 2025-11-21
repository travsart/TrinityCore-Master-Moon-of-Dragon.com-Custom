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
#include "Group.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <utility>
#include <atomic>
#include <chrono>
#include "GameTime.h"

namespace Playerbot
{

// Enum definitions (needs full definition for struct members)
enum class GuildEventType : uint8
{
    RAID_DUNGEON        = 0,
    PVP_BATTLEGROUND    = 1,
    PVP_ARENA           = 2,
    GUILD_MEETING       = 3,
    SOCIAL_GATHERING    = 4,
    ACHIEVEMENT_RUN     = 5,
    LEVELING_GROUP      = 6,
    CRAFTING_SESSION    = 7,
    CONTEST_COMPETITION = 8,
    OFFICER_MEETING     = 9
};

enum class EventStatus : uint8
{
    PLANNING        = 0,
    RECRUITING      = 1,
    CONFIRMED       = 2,
    IN_PROGRESS     = 3,
    COMPLETED       = 4,
    CANCELLED       = 5,
    POSTPONED       = 6
};

// Event coordination profile
struct EventCoordinationProfile
{
    bool enableEventPlanning;
    bool enableEventParticipation;
    bool enableEventLeadership;
    std::vector<GuildEventType> preferredEventTypes;
    std::vector<GuildEventType> availableLeadershipTypes;
    float planningProactiveness; // How often to propose events
    float participationRate; // Likelihood to join events
    std::vector<std::pair<uint32, uint32>> availabilityWindows; // startTime, endTime pairs
    uint32 maxEventsPerWeek;
    bool autoAcceptInvitations;

    EventCoordinationProfile() : enableEventPlanning(true), enableEventParticipation(true)
        , enableEventLeadership(false), planningProactiveness(0.3f)
        , participationRate(0.8f), maxEventsPerWeek(7), autoAcceptInvitations(false) {}
};

// Event participation tracking
struct EventParticipation
{
    uint32 playerGuid;
    uint32 guildId;
    std::vector<uint32> organizedEvents;
    std::vector<uint32> participatedEvents;
    std::unordered_map<GuildEventType, uint32> eventTypePreferences;
    uint32 totalEventsCreated;
    uint32 totalEventsAttended;
    float organizationRating;
    float participationRating;
    uint32 lastEventActivity;

    EventParticipation(uint32 pGuid, uint32 gId) : playerGuid(pGuid), guildId(gId)
        , totalEventsCreated(0), totalEventsAttended(0), organizationRating(0.5f)
        , participationRating(0.7f), lastEventActivity(GameTime::GetGameTimeMS()) {}
};

// Event metrics tracking
struct EventMetrics
{
    std::atomic<uint32> eventsCreated{0};
    std::atomic<uint32> eventsCompleted{0};
    std::atomic<uint32> eventsCancelled{0};
    std::atomic<uint32> totalParticipants{0};
    std::atomic<float> averageAttendance{0.75f};
    std::atomic<float> organizationEfficiency{0.8f};
    std::atomic<float> memberSatisfaction{0.85f};
    std::chrono::steady_clock::time_point lastUpdate;

    // Default constructor
    EventMetrics() = default;

    // Copy constructor for atomic members
    EventMetrics(const EventMetrics& other) :
        eventsCreated(other.eventsCreated.load()),
        eventsCompleted(other.eventsCompleted.load()),
        eventsCancelled(other.eventsCancelled.load()),
        totalParticipants(other.totalParticipants.load()),
        averageAttendance(other.averageAttendance.load()),
        organizationEfficiency(other.organizationEfficiency.load()),
        memberSatisfaction(other.memberSatisfaction.load()),
        lastUpdate(other.lastUpdate) {}

    // Assignment operator for atomic members
    EventMetrics& operator=(const EventMetrics& other) {
        if (this != &other) {
            eventsCreated.store(other.eventsCreated.load());
            eventsCompleted.store(other.eventsCompleted.load());
            eventsCancelled.store(other.eventsCancelled.load());
            totalParticipants.store(other.totalParticipants.load());
            averageAttendance.store(other.averageAttendance.load());
            organizationEfficiency.store(other.organizationEfficiency.load());
            memberSatisfaction.store(other.memberSatisfaction.load());
            lastUpdate = other.lastUpdate;
        }
        return *this;
    }

    void Reset() {
        eventsCreated = 0; eventsCompleted = 0; eventsCancelled = 0;
        totalParticipants = 0; averageAttendance = 0.75f;
        organizationEfficiency = 0.8f; memberSatisfaction = 0.85f;
        lastUpdate = std::chrono::steady_clock::now();
    }

    float GetCompletionRate() const {
        uint32 created = eventsCreated.load();
        uint32 completed = eventsCompleted.load();
        return created > 0 ? (float)completed / created : 0.0f;
    }
};

// Forward declarations
struct GuildEvent;

class TC_GAME_API IGuildEventCoordinator
{
public:
    virtual ~IGuildEventCoordinator() = default;

    // Core event management using TrinityCore's Calendar system
    virtual uint32 CreateGuildEvent(Player* organizer, const GuildEvent& eventData) = 0;
    virtual bool UpdateGuildEvent(uint32 eventId, const GuildEvent& updatedData) = 0;
    virtual bool CancelGuildEvent(Player* organizer, uint32 eventId) = 0;
    virtual void ProcessEventInvitations(uint32 eventId) = 0;

    // Event planning and scheduling
    virtual void PlanGuildEvents() = 0;
    virtual void ScheduleRecurringEvents() = 0;

    // Event recruitment and coordination
    virtual void RecruitEventParticipants(Player* organizer, uint32 eventId) = 0;
    virtual void ManageEventSignups(uint32 eventId) = 0;
    virtual void AssignEventRoles(uint32 eventId) = 0;

    // Event execution and management
    virtual void ExecuteGuildEvent(uint32 eventId) = 0;
    virtual void CoordinateEventActivities(Player* leader, uint32 eventId) = 0;
    virtual void MonitorEventProgress(uint32 eventId) = 0;
    virtual void HandleEventCompletion(uint32 eventId) = 0;

    // Event profiles
    virtual void SetEventProfile(const EventCoordinationProfile& profile) = 0;
    virtual EventCoordinationProfile GetEventProfile() = 0;

    // Event analytics
    virtual EventParticipation GetEventParticipation() = 0;

    // Event optimization
    virtual void OptimizeEventScheduling() = 0;
    virtual void AnalyzeGuildEventPatterns(uint32 guildId) = 0;

    // Event communication
    virtual void BroadcastEventUpdates(uint32 eventId, const ::std::string& updateMessage) = 0;
    virtual void SendEventReminders(uint32 eventId) = 0;
    virtual void UpdateEventStatus(uint32 eventId, EventStatus newStatus) = 0;

    // Group formation
    virtual Group* FormEventGroup(uint32 eventId) = 0;
    virtual void AssignGroupRoles(Group* group, uint32 eventId) = 0;

    // Performance monitoring
    virtual EventMetrics GetGuildEventMetrics(uint32 guildId) = 0;
    virtual EventMetrics GetPlayerEventMetrics() = 0;

    // Configuration
    virtual void SetEventCoordinationEnabled(uint32 guildId, bool enabled) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void UpdateEventStates() = 0;
    virtual void CleanupExpiredEvents() = 0;
};

} // namespace Playerbot
