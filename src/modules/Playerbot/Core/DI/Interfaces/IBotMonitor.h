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

#ifndef _IBOTMONITOR_H
#define _IBOTMONITOR_H

#include "Define.h"
#include "ObjectGuid.h"
#include "Monitoring/PerformanceMetrics.h"
#include <vector>
#include <string>
#include <functional>

namespace Playerbot
{
    /**
     * @brief Interface for Bot Monitor
     *
     * Central monitoring system for playerbot performance and health.
     * Provides real-time metrics collection, trend analysis, alerting, and
     * performance snapshot capabilities for the playerbot system.
     *
     * **Responsibilities:**
     * - Performance snapshot collection and history
     * - Bot activity tracking (combat, questing, deaths)
     * - Resource monitoring (CPU, memory, database)
     * - Trend analysis for key metrics
     * - Alert management and callbacks
     * - Statistics reporting and summaries
     */
    class TC_GAME_API IBotMonitor
    {
    public:
        virtual ~IBotMonitor() = default;

        /**
         * @brief Initialize monitoring system
         * @return true if initialization successful
         */
        virtual bool Initialize() = 0;

        /**
         * @brief Shutdown monitoring system
         */
        virtual void Shutdown() = 0;

        /**
         * @brief Update monitoring system (called periodically)
         * @param diff Time since last update in milliseconds
         */
        virtual void Update(uint32 diff) = 0;

        // =====================================================================
        // METRICS COLLECTION
        // =====================================================================

        /**
         * @brief Capture current performance snapshot
         * @return Current performance metrics snapshot
         */
        virtual PerformanceSnapshot CaptureSnapshot() = 0;

        /**
         * @brief Get most recent snapshot
         * @return Most recent performance snapshot
         */
        virtual PerformanceSnapshot GetLatestSnapshot() const = 0;

        /**
         * @brief Get historical snapshots
         * @param count Number of snapshots to retrieve (default: all)
         * @return Vector of historical snapshots (newest first)
         */
        virtual std::vector<PerformanceSnapshot> GetSnapshotHistory(uint32 count = 0) const = 0;

        // =====================================================================
        // ACTIVITY TRACKING
        // =====================================================================

        /**
         * @brief Record bot entering combat
         * @param botGuid Bot's ObjectGuid
         */
        virtual void RecordBotCombatStart(ObjectGuid botGuid) = 0;

        /**
         * @brief Record bot leaving combat
         * @param botGuid Bot's ObjectGuid
         */
        virtual void RecordBotCombatEnd(ObjectGuid botGuid) = 0;

        /**
         * @brief Record bot starting quest
         * @param botGuid Bot's ObjectGuid
         */
        virtual void RecordBotQuestStart(ObjectGuid botGuid) = 0;

        /**
         * @brief Record bot completing quest
         * @param botGuid Bot's ObjectGuid
         */
        virtual void RecordBotQuestEnd(ObjectGuid botGuid) = 0;

        /**
         * @brief Record bot death
         * @param botGuid Bot's ObjectGuid
         */
        virtual void RecordBotDeath(ObjectGuid botGuid) = 0;

        /**
         * @brief Record bot resurrection
         * @param botGuid Bot's ObjectGuid
         */
        virtual void RecordBotResurrection(ObjectGuid botGuid) = 0;

        /**
         * @brief Record bot update timing
         * @param botGuid Bot's ObjectGuid
         * @param updateTimeMs Update time in milliseconds
         */
        virtual void RecordBotUpdateTime(ObjectGuid botGuid, double updateTimeMs) = 0;

        /**
         * @brief Record AI decision timing
         * @param botGuid Bot's ObjectGuid
         * @param decisionTimeMs Decision time in milliseconds
         */
        virtual void RecordAIDecisionTime(ObjectGuid botGuid, double decisionTimeMs) = 0;

        // =====================================================================
        // RESOURCE TRACKING
        // =====================================================================

        /**
         * @brief Record database query execution
         * @param queryTimeMs Query execution time in milliseconds
         */
        virtual void RecordDatabaseQuery(double queryTimeMs) = 0;

        /**
         * @brief Record database cache hit
         */
        virtual void RecordDatabaseCacheHit() = 0;

        /**
         * @brief Record database cache miss
         */
        virtual void RecordDatabaseCacheMiss() = 0;

        /**
         * @brief Record error occurrence
         * @param category Error category (e.g., "Combat", "Movement", "Database")
         * @param message Error message
         */
        virtual void RecordError(std::string const& category, std::string const& message) = 0;

        /**
         * @brief Record warning occurrence
         * @param category Warning category
         * @param message Warning message
         */
        virtual void RecordWarning(std::string const& category, std::string const& message) = 0;

        // =====================================================================
        // TREND ANALYSIS
        // =====================================================================

        /**
         * @brief Get CPU usage trend data
         * @return TrendData for CPU usage over time
         */
        virtual TrendData GetCpuTrend() const = 0;

        /**
         * @brief Get memory usage trend data
         * @return TrendData for memory usage over time
         */
        virtual TrendData GetMemoryTrend() const = 0;

        /**
         * @brief Get active bot count trend data
         * @return TrendData for bot count over time
         */
        virtual TrendData GetBotCountTrend() const = 0;

        /**
         * @brief Get database query performance trend
         * @return TrendData for database query times
         */
        virtual TrendData GetQueryTimeTrend() const = 0;

        // =====================================================================
        // ALERT MANAGEMENT
        // =====================================================================

        /**
         * @brief Get current alert thresholds
         * @return Current alert threshold configuration
         */
        virtual AlertThresholds GetAlertThresholds() const = 0;

        /**
         * @brief Set alert thresholds
         * @param thresholds New alert threshold configuration
         */
        virtual void SetAlertThresholds(AlertThresholds const& thresholds) = 0;

        /**
         * @brief Get active alerts
         * @param minLevel Minimum alert level to retrieve (default: WARNING)
         * @return Vector of active alerts
         */
        virtual std::vector<PerformanceAlert> GetActiveAlerts(AlertLevel minLevel = AlertLevel::WARNING) const = 0;

        /**
         * @brief Get alert history
         * @param count Number of alerts to retrieve (default: 100)
         * @return Vector of historical alerts (newest first)
         */
        virtual std::vector<PerformanceAlert> GetAlertHistory(uint32 count = 100) const = 0;

        /**
         * @brief Clear alert history
         */
        virtual void ClearAlertHistory() = 0;

        /**
         * @brief Register alert callback
         * @param callback Function to call when alert is triggered
         */
        virtual void RegisterAlertCallback(std::function<void(PerformanceAlert const&)> callback) = 0;

        // =====================================================================
        // STATISTICS
        // =====================================================================

        /**
         * @brief Get formatted statistics summary
         * @return Human-readable statistics string
         */
        virtual std::string GetStatisticsSummary() const = 0;

        /**
         * @brief Get uptime in seconds
         * @return System uptime since initialization
         */
        virtual uint64 GetUptimeSeconds() const = 0;

        /**
         * @brief Reset all statistics
         */
        virtual void ResetStatistics() = 0;
    };

} // namespace Playerbot

#endif // _IBOTMONITOR_H
