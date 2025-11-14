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
#include <variant>
#include <map>
#include <optional>
#include <functional>

namespace Playerbot
{

/**
 * @brief Interface for Runtime Configuration Management
 *
 * Abstracts bot configuration management to enable dependency injection and testing.
 *
 * **Responsibilities:**
 * - Runtime configuration value modification
 * - Type-safe configuration storage and retrieval
 * - Configuration validation and persistence
 * - Change notification callbacks
 *
 * **Testability:**
 * - Can be mocked for testing with predefined configuration values
 * - Enables isolated testing without file I/O
 *
 * Example:
 * @code
 * auto configMgr = Services::Container().Resolve<IConfigManager>();
 * uint32 maxBots = configMgr->GetUInt("MaxActiveBots", 100);
 * configMgr->SetValue("MaxActiveBots", 200);
 * @endcode
 */
class TC_GAME_API IConfigManager
{
public:
    using ConfigValue = ::std::variant<bool, int32, uint32, float, ::std::string>;
    using ChangeCallback = ::std::function<void(ConfigValue const&)>;

    struct ValidationRule
    {
        ::std::string key;
        ::std::function<bool(ConfigValue const&)> validator;
        ::std::string errorMessage;
    };

    struct ConfigEntry
    {
        ConfigValue value;
        ::std::string description;
        ConfigValue defaultValue;
        bool persistent;
        bool readOnly;
    };

    virtual ~IConfigManager() = default;

    /**
     * @brief Initialize configuration manager
     * @return true if successful, false otherwise
     */
    virtual bool Initialize() = 0;

    /**
     * @brief Set configuration value
     * @param key Configuration key
     * @param value New value
     * @return true if successful, false if validation failed
     */
    virtual bool SetValue(::std::string const& key, ConfigValue const& value) = 0;

    /**
     * @brief Get boolean configuration value
     * @param key Configuration key
     * @param defaultValue Default value if not found
     * @return Configuration value or default
     */
    virtual bool GetBool(::std::string const& key, bool defaultValue) const = 0;

    /**
     * @brief Get signed integer configuration value
     * @param key Configuration key
     * @param defaultValue Default value if not found
     * @return Configuration value or default
     */
    virtual int32 GetInt(::std::string const& key, int32 defaultValue) const = 0;

    /**
     * @brief Get unsigned integer configuration value
     * @param key Configuration key
     * @param defaultValue Default value if not found
     * @return Configuration value or default
     */
    virtual uint32 GetUInt(::std::string const& key, uint32 defaultValue) const = 0;

    /**
     * @brief Get float configuration value
     * @param key Configuration key
     * @param defaultValue Default value if not found
     * @return Configuration value or default
     */
    virtual float GetFloat(::std::string const& key, float defaultValue) const = 0;

    /**
     * @brief Get string configuration value
     * @param key Configuration key
     * @param defaultValue Default value if not found
     * @return Configuration value or default
     */
    virtual ::std::string GetString(::std::string const& key, ::std::string const& defaultValue) const = 0;

    /**
     * @brief Register configuration change callback
     * @param key Configuration key to monitor
     * @param callback Function to call when value changes
     */
    virtual void RegisterCallback(::std::string const& key, ChangeCallback callback) = 0;

    /**
     * @brief Get all configuration entries
     * @return Map of all configuration entries
     */
    virtual ::std::map<::std::string, ConfigEntry> GetAllEntries() const = 0;

    /**
     * @brief Reset configuration to defaults
     */
    virtual void ResetToDefaults() = 0;

    /**
     * @brief Save configuration to file
     * @param filePath Path to configuration file (optional)
     * @return true if successful, false otherwise
     */
    virtual bool SaveToFile(::std::string const& filePath = "") const = 0;

    /**
     * @brief Load configuration from file
     * @param filePath Path to configuration file
     * @return true if successful, false otherwise
     */
    virtual bool LoadFromFile(::std::string const& filePath) = 0;

    /**
     * @brief Get last error message
     * @return Error description
     */
    virtual ::std::string GetLastError() const = 0;

    /**
     * @brief Check if configuration key exists
     * @param key Configuration key
     * @return true if exists, false otherwise
     */
    virtual bool HasKey(::std::string const& key) const = 0;

    /**
     * @brief Get configuration entry (with metadata)
     * @param key Configuration key
     * @return Configuration entry or nullopt if not found
     */
    virtual ::std::optional<ConfigEntry> GetEntry(::std::string const& key) const = 0;
};

} // namespace Playerbot
