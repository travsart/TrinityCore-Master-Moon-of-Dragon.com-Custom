/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include <string>
#include <vector>
#include <chrono>

namespace Playerbot
{

// Forward declarations
struct ScheduleEntry;
struct ActivityPattern;
struct BotScheduleState;
struct SchedulerConfig;
struct SchedulerStats;

/**
 * @brief Interface for Bot Scheduler
 *
 * Manages bot lifecycle scheduling and activity patterns with:
 * - Realistic login/logout scheduling
 * - Activity pattern management
 * - Time-based bot lifecycle
 * - Schedule persistence
 *
 * Thread Safety: All methods are thread-safe
 */
class TC_GAME_API IBotScheduler
{
public:
    virtual ~IBotScheduler() = default;

    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Update(uint32 diff) = 0;

    // Configuration
    virtual void LoadConfig() = 0;
    virtual SchedulerConfig const& GetConfig() const = 0;
    virtual void SetConfig(SchedulerConfig const& config) = 0;

    // Activity patterns
    virtual void LoadActivityPatterns() = 0;
    virtual void RegisterPattern(std::string const& name, ActivityPattern const& pattern) = 0;
    virtual ActivityPattern const* GetPattern(std::string const& name) const = 0;
    virtual std::vector<std::string> GetAvailablePatterns() const = 0;
    virtual bool RemovePattern(std::string const& name) = 0;

    // Scheduling operations
    virtual void ScheduleBot(ObjectGuid guid, std::string const& patternName = "default") = 0;
    virtual void UnscheduleBot(ObjectGuid guid) = 0;
    virtual void ScheduleAction(ScheduleEntry const& entry) = 0;
    virtual void ScheduleLogin(ObjectGuid guid, std::chrono::system_clock::time_point when) = 0;
    virtual void ScheduleLogout(ObjectGuid guid, std::chrono::system_clock::time_point when) = 0;

    // Pattern management
    virtual void AssignPattern(ObjectGuid guid, std::string const& patternName) = 0;
    virtual std::string GetBotPattern(ObjectGuid guid) const = 0;
    virtual BotScheduleState const* GetBotScheduleState(ObjectGuid guid) const = 0;

    // Time calculations
    virtual std::chrono::system_clock::time_point CalculateNextLogin(ObjectGuid guid) = 0;
    virtual std::chrono::system_clock::time_point CalculateNextLogout(ObjectGuid guid) = 0;

    // Schedule processing
    virtual void ProcessSchedule() = 0;
    virtual void ExecuteScheduledAction(ScheduleEntry const& entry) = 0;

    // Status queries
    virtual bool IsBotScheduled(ObjectGuid guid) const = 0;
    virtual bool IsBotActive(ObjectGuid guid) const = 0;
    virtual uint32 GetScheduledBotCount() const = 0;

    // Statistics
    virtual SchedulerStats const& GetStats() const = 0;
    virtual void ResetStats() = 0;

    // Event callbacks
    virtual void OnBotLoggedIn(ObjectGuid guid) = 0;
    virtual void OnBotLoginFailed(ObjectGuid guid, std::string const& reason = "") = 0;

    // Control
    virtual void SetEnabled(bool enabled) = 0;
    virtual bool IsEnabled() const = 0;

    // Debugging
    virtual void DumpSchedule() const = 0;
    virtual bool ValidateSchedule() const = 0;
};

} // namespace Playerbot
