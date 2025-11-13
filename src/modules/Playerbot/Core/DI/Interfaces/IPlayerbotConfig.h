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
#include <string>

/**
 * @brief Performance metrics structure for configuration access
 */
struct PerformanceMetrics
{
    uint64 configLookups = 0;
    uint64 cacheHits = 0;
    uint64 cacheMisses = 0;
    uint32 cacheHitRate() const { return configLookups > 0 ? (cacheHits * 100) / configLookups : 0; }
};

/**
 * @brief Interface for Playerbot configuration management
 *
 * Provides type-safe access to playerbot configuration values with:
 * - Configuration file loading and hot-reload
 * - Type-safe value access (bool, int, uint, float, string)
 * - Performance caching for frequently accessed values
 * - Validation of configuration settings
 * - Integration with TrinityCore logging system
 *
 * Thread Safety: All read methods are thread-safe
 */
class TC_GAME_API IPlayerbotConfig
{
public:
    virtual ~IPlayerbotConfig() = default;

    // ====================================================================
    // INITIALIZATION
    // ====================================================================

    /**
     * @brief Initialize configuration system
     * @return true if successful, false otherwise
     */
    virtual bool Initialize() = 0;

    /**
     * @brief Reload configuration from file
     * @return true if successful, false otherwise
     */
    virtual bool Reload() = 0;

    /**
     * @brief Check if configuration is loaded and valid
     * @return true if valid, false otherwise
     */
    virtual bool IsValid() const = 0;

    // ====================================================================
    // CONFIGURATION ACCESS
    // ====================================================================

    /**
     * @brief Get boolean configuration value
     * @param key Configuration key
     * @param defaultValue Default value if key not found
     * @return Configuration value or default
     */
    virtual bool GetBool(std::string const& key, bool defaultValue) const = 0;

    /**
     * @brief Get integer configuration value
     * @param key Configuration key
     * @param defaultValue Default value if key not found
     * @return Configuration value or default
     */
    virtual int32 GetInt(std::string const& key, int32 defaultValue) const = 0;

    /**
     * @brief Get unsigned integer configuration value
     * @param key Configuration key
     * @param defaultValue Default value if key not found
     * @return Configuration value or default
     */
    virtual uint32 GetUInt(std::string const& key, uint32 defaultValue) const = 0;

    /**
     * @brief Get float configuration value
     * @param key Configuration key
     * @param defaultValue Default value if key not found
     * @return Configuration value or default
     */
    virtual float GetFloat(std::string const& key, float defaultValue) const = 0;

    /**
     * @brief Get string configuration value
     * @param key Configuration key
     * @param defaultValue Default value if key not found
     * @return Configuration value or default
     */
    virtual std::string GetString(std::string const& key, std::string const& defaultValue) const = 0;

    /**
     * @brief Get configuration file path
     * @return Path to playerbots.conf file
     */
    virtual std::string GetConfigPath() const = 0;

    /**
     * @brief Get last error message
     * @return Error description or empty string
     */
    virtual std::string GetLastError() const = 0;

    // ====================================================================
    // LOGGING INTEGRATION
    // ====================================================================

    /**
     * @brief Initialize playerbot logging system
     *
     * Sets up TrinityCore logging integration with:
     * - Separate Playerbot.log file
     * - Configurable log levels from playerbots.conf
     * - Specialized logging categories for different subsystems
     */
    virtual void InitializeLogging() = 0;

    // ====================================================================
    // PERFORMANCE OPTIMIZATION
    // ====================================================================

    /**
     * @brief Refresh configuration cache for frequently accessed values
     */
    virtual void RefreshCache() = 0;

    /**
     * @brief Get performance metrics for monitoring
     * @return Performance statistics
     */
    virtual PerformanceMetrics GetPerformanceMetrics() const = 0;
};
