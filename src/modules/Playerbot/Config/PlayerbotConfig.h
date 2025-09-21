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

#ifndef TRINITY_PLAYERBOTCONFIG_H
#define TRINITY_PLAYERBOTCONFIG_H

#ifdef PLAYERBOT_ENABLED

#include "Define.h"
#include "ModuleLogManager.h"
#include <map>
#include <string>

/**
 * @class PlayerbotConfig
 * @brief Configuration manager for the Playerbot module
 *
 * This singleton class manages all configuration settings for the playerbot system.
 * It loads settings from playerbots.conf (NOT worldserver.conf) and provides
 * type-safe access to configuration values with reasonable defaults.
 *
 * Key Features:
 * - Separate configuration file (playerbots.conf)
 * - Hot-reload capability
 * - Type-safe configuration access
 * - Default value fallbacks
 * - Configuration validation
 * - Zero impact on core TrinityCore configuration
 */
class TC_GAME_API PlayerbotConfig
{
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the configuration instance
     */
    static PlayerbotConfig* instance();

    /**
     * @brief Initialize the configuration system
     * @return true if successful, false otherwise
     */
    bool Initialize();

    /**
     * @brief Reload configuration from file
     * @return true if successful, false otherwise
     */
    bool Reload();

    /**
     * @brief Check if configuration is loaded and valid
     * @return true if valid, false otherwise
     */
    bool IsValid() const { return _loaded; }

    // Configuration value getters with type safety and defaults

    /**
     * @brief Get boolean configuration value
     * @param key Configuration key
     * @param defaultValue Default value if key not found
     * @return Configuration value or default
     */
    bool GetBool(std::string const& key, bool defaultValue) const;

    /**
     * @brief Get integer configuration value
     * @param key Configuration key
     * @param defaultValue Default value if key not found
     * @return Configuration value or default
     */
    int32 GetInt(std::string const& key, int32 defaultValue) const;

    /**
     * @brief Get unsigned integer configuration value
     * @param key Configuration key
     * @param defaultValue Default value if key not found
     * @return Configuration value or default
     */
    uint32 GetUInt(std::string const& key, uint32 defaultValue) const;

    /**
     * @brief Get float configuration value
     * @param key Configuration key
     * @param defaultValue Default value if key not found
     * @return Configuration value or default
     */
    float GetFloat(std::string const& key, float defaultValue) const;

    /**
     * @brief Get string configuration value
     * @param key Configuration key
     * @param defaultValue Default value if key not found
     * @return Configuration value or default
     */
    std::string GetString(std::string const& key, std::string const& defaultValue) const;

    /**
     * @brief Get configuration file path
     * @return Path to playerbots.conf file
     */
    std::string GetConfigPath() const { return _configPath; }

    /**
     * @brief Get last error message
     * @return Error description or empty string
     */
    std::string GetLastError() const { return _lastError; }

    /**
     * @brief Initialize the playerbot logging system
     *
     * Sets up TrinityCore logging integration with:
     * - Separate Playerbot.log file
     * - Configurable log levels from playerbots.conf
     * - Specialized logging categories for different subsystems
     */
    void InitializeLogging();

    /**
     * @brief Setup playerbot-specific logging configuration
     * @param logLevel Base log level for all playerbot loggers
     * @param logFile Path to the playerbot log file
     */
    void SetupPlayerbotLogging(int32 logLevel, std::string const& logFile);

    /**
     * @brief Create specialized loggers for different subsystems
     * @param baseLevel Base log level to use for specialized loggers
     */
    void CreateSpecializedLoggers(int32 baseLevel);

private:
    PlayerbotConfig() = default;
    ~PlayerbotConfig() = default;

    // Non-copyable
    PlayerbotConfig(const PlayerbotConfig&) = delete;
    PlayerbotConfig& operator=(const PlayerbotConfig&) = delete;

    /**
     * @brief Find configuration file location
     * @return Path to configuration file
     */
    std::string FindConfigFile() const;

    /**
     * @brief Load configuration from file
     * @param filePath Path to configuration file
     * @return true if successful, false otherwise
     */
    bool LoadConfigFile(const std::string& filePath);

    /**
     * @brief Validate loaded configuration
     * @return true if valid, false otherwise
     */
    bool ValidateConfiguration();

    // Configuration state
    std::map<std::string, std::string> _configValues;
    std::string _configPath;
    std::string _lastError;
    bool _loaded = false;

    // Default configuration filename
    static constexpr const char* CONFIG_FILENAME = "playerbots.conf";
};

// Global accessor
#define sPlayerbotConfig PlayerbotConfig::instance()

#endif // PLAYERBOT_ENABLED
#endif // TRINITY_PLAYERBOTCONFIG_H