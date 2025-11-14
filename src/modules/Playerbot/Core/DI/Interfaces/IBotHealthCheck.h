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
#include <vector>
#include <string>

namespace Playerbot
{

// Forward declarations
enum class HealthStatus : uint8;
struct HealthCheckResult;

/**
 * @brief Interface for Bot Health Monitoring
 *
 * Enterprise-grade health checks and anomaly detection with:
 * - Stall detection (bots not updating)
 * - Deadlock detection (system-wide hangs)
 * - Error rate monitoring
 * - Automatic recovery mechanisms
 * - Health status reporting
 *
 * Thread Safety: All methods are thread-safe
 */
class TC_GAME_API IBotHealthCheck
{
public:
    virtual ~IBotHealthCheck() = default;

    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void PerformHealthChecks(uint32 currentTime) = 0;

    // Stall detection
    virtual void CheckForStalledBots(uint32 currentTime) = 0;
    virtual ::std::vector<ObjectGuid> GetStalledBots() const = 0;
    virtual bool IsBotStalled(ObjectGuid botGuid) const = 0;

    // Deadlock detection
    virtual void CheckForDeadlocks(uint32 currentTime) = 0;
    virtual bool IsSystemDeadlocked() const = 0;
    virtual uint32 GetTimeSinceLastProgress() const = 0;

    // Error monitoring
    virtual void RecordError(ObjectGuid botGuid, ::std::string const& errorType) = 0;
    virtual float GetSystemErrorRate() const = 0;
    virtual bool IsErrorRateExcessive() const = 0;

    // Health status
    virtual HealthStatus GetSystemHealth() const = 0;
    virtual HealthStatus GetBotHealth(ObjectGuid botGuid) const = 0;
    virtual ::std::vector<HealthCheckResult> GetRecentHealthIssues() const = 0;

    // Recovery
    virtual void TriggerAutomaticRecovery(ObjectGuid botGuid) = 0;
    virtual void TriggerSystemRecovery() = 0;

    // Configuration
    virtual void SetStallThreshold(uint32 milliseconds) = 0;
    virtual void SetDeadlockThreshold(uint32 milliseconds) = 0;
    virtual void SetErrorRateThreshold(float errorsPerSecond) = 0;
    virtual void SetAutoRecoveryEnabled(bool enabled) = 0;
    virtual uint32 GetStallThreshold() const = 0;
    virtual uint32 GetDeadlockThreshold() const = 0;
    virtual float GetErrorRateThreshold() const = 0;
    virtual bool IsAutoRecoveryEnabled() const = 0;

    // Heartbeat
    virtual void RecordHeartbeat(uint32 currentTime) = 0;

    // Reporting
    virtual void LogHealthReport() const = 0;
    virtual void LogDetailedHealthStatus() const = 0;

    // Administrative
    virtual void ClearStalledBot(ObjectGuid botGuid) = 0;
    virtual void ClearAllHealthIssues() = 0;
};

} // namespace Playerbot
