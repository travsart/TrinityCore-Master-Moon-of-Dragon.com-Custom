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

#ifdef PLAYERBOT_ENABLED

#include "PlayerbotConfig.h"
#include "Log.h"
#include "Util.h"
#include "StringFormat.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

PlayerbotConfig* PlayerbotConfig::instance()
{
    static PlayerbotConfig instance;
    return &instance;
}

bool PlayerbotConfig::Initialize()
{
    _loaded = false;
    _lastError.clear();

    // Find configuration file
    _configPath = FindConfigFile();
    if (_configPath.empty())
    {
        _lastError = "Could not find playerbots.conf file";
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: {}", _lastError);
        return false;
    }

    // Load configuration
    if (!LoadConfigFile(_configPath))
    {
        _lastError = Trinity::StringFormat("Failed to load configuration from {}", _configPath);
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: {}", _lastError);
        return false;
    }

    // Validate configuration
    if (!ValidateConfiguration())
    {
        _lastError = "Configuration validation failed";
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: {}", _lastError);
        return false;
    }

    _loaded = true;
    TC_LOG_INFO("server.loading", "PlayerbotConfig: Successfully loaded from {}", _configPath);
    return true;
}

bool PlayerbotConfig::Reload()
{
    std::lock_guard<std::mutex> lock(_configMutex);
    if (_configPath.empty())
    {
        _lastError = "Configuration not initialized";
        return false;
    }

    // Invalidate cache during reload
    _cache.isValid = false;

    _configValues.clear();
    if (!LoadConfigFile(_configPath))
    {
        _lastError = "Failed to reload configuration file";
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: {}", _lastError);
        return false;
    }

    if (!ValidateConfiguration())
    {
        _lastError = "Configuration validation failed after reload";
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: {}", _lastError);
        return false;
    }

    TC_LOG_INFO("server.loading", "PlayerbotConfig: Configuration reloaded successfully");
    return true;
}

bool PlayerbotConfig::GetBool(std::string const& key, bool defaultValue) const
{
    std::lock_guard<std::mutex> lock(_configMutex);
    auto it = _configValues.find(key);
    if (it == _configValues.end())
        return defaultValue;

    std::string value = it->second;
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    return (value == "1" || value == "true" || value == "yes" || value == "on");
}

int32 PlayerbotConfig::GetInt(std::string const& key, int32 defaultValue) const
{
    std::lock_guard<std::mutex> lock(_configMutex);
    auto it = _configValues.find(key);
    if (it == _configValues.end())
        return defaultValue;

    try {
        return std::stoi(it->second);
    } catch (const std::invalid_argument& e) {
        TC_LOG_WARN("server.loading", "PlayerbotConfig: Invalid integer value '{}' for key '{}', using default {}",
                    it->second, key, defaultValue);
        return defaultValue;
    } catch (const std::out_of_range& e) {
        TC_LOG_WARN("server.loading", "PlayerbotConfig: Integer value '{}' for key '{}' out of range, using default {}",
                    it->second, key, defaultValue);
        return defaultValue;
    }
}

uint32 PlayerbotConfig::GetUInt(std::string const& key, uint32 defaultValue) const
{
    std::lock_guard<std::mutex> lock(_configMutex);
    auto it = _configValues.find(key);
    if (it == _configValues.end())
        return defaultValue;

    try {
        return static_cast<uint32>(std::stoul(it->second));
    } catch (const std::invalid_argument& e) {
        TC_LOG_WARN("server.loading", "PlayerbotConfig: Invalid unsigned integer value '{}' for key '{}', using default {}",
                    it->second, key, defaultValue);
        return defaultValue;
    } catch (const std::out_of_range& e) {
        TC_LOG_WARN("server.loading", "PlayerbotConfig: Unsigned integer value '{}' for key '{}' out of range, using default {}",
                    it->second, key, defaultValue);
        return defaultValue;
    }
}

float PlayerbotConfig::GetFloat(std::string const& key, float defaultValue) const
{
    std::lock_guard<std::mutex> lock(_configMutex);
    auto it = _configValues.find(key);
    if (it == _configValues.end())
        return defaultValue;

    try {
        return std::stof(it->second);
    } catch (const std::invalid_argument& e) {
        TC_LOG_WARN("server.loading", "PlayerbotConfig: Invalid float value '{}' for key '{}', using default {}",
                    it->second, key, defaultValue);
        return defaultValue;
    } catch (const std::out_of_range& e) {
        TC_LOG_WARN("server.loading", "PlayerbotConfig: Float value '{}' for key '{}' out of range, using default {}",
                    it->second, key, defaultValue);
        return defaultValue;
    }
}

std::string PlayerbotConfig::GetString(std::string const& key, std::string const& defaultValue) const
{
    std::lock_guard<std::mutex> lock(_configMutex);
    auto it = _configValues.find(key);
    if (it == _configValues.end())
        return defaultValue;

    return it->second;
}

std::string PlayerbotConfig::FindConfigFile() const
{
    // List of potential configuration file locations (in order of preference)
    std::vector<std::string> searchPaths = {
        "./playerbots.conf",                    // Current directory
        "./etc/playerbots.conf",                // etc subdirectory
        "../etc/playerbots.conf",               // Parent etc directory
        "/usr/local/etc/playerbots.conf",       // System etc directory
        "./playerbots.conf.dist"                // Distribution template as fallback
    };

    for (const std::string& path : searchPaths)
    {
        if (std::filesystem::exists(path))
        {
            TC_LOG_DEBUG("server.loading", "PlayerbotConfig: Found config file at {}", path);
            return path;
        }
    }

    return "";
}

bool PlayerbotConfig::LoadConfigFile(const std::string& filePath)
{
    // RAII file management - automatically closes on scope exit
    std::ifstream file(filePath);

    if (!file.is_open())
    {
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: Failed to open configuration file: {}", filePath);
        return false;
    }

    // Check if file is readable
    if (!file.good())
    {
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: Configuration file is not readable: {}", filePath);
        return false;
    }

    std::string line;
    int lineCount = 0;

    try {
        while (std::getline(file, line))
        {
            lineCount++;

            // Skip empty lines and comments
            if (line.empty() || line[0] == '#')
                continue;

            // Find the = separator
            size_t pos = line.find('=');
            if (pos == std::string::npos)
            {
                TC_LOG_WARN("server.loading", "PlayerbotConfig: Malformed line {} in {}: missing '=' separator",
                           lineCount, filePath);
                continue;
            }

            // Extract key and value
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            // Trim whitespace
            key.erase(key.find_last_not_of(" \t") + 1);
            key.erase(0, key.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));

            // Validate key is not empty
            if (key.empty())
            {
                TC_LOG_WARN("server.loading", "PlayerbotConfig: Empty key on line {} in {}", lineCount, filePath);
                continue;
            }

            // Store the value
            _configValues[key] = value;
        }
    }
    catch (const std::exception& e)
    {
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: Exception while reading {}: {}", filePath, e.what());
        return false;
    }

    // File automatically closed by RAII destructor
    TC_LOG_DEBUG("server.loading", "PlayerbotConfig: Successfully loaded {} configuration entries from {}",
                 _configValues.size(), filePath);
    return true;
}

bool PlayerbotConfig::ValidateConfiguration()
{
    // Validate critical settings
    bool valid = true;

    // Validate core playerbot settings
    if (!ValidateBotLimits()) valid = false;
    if (!ValidateTimingSettings()) valid = false;
    if (!ValidateLoggingSettings()) valid = false;
    if (!ValidateDatabaseSettings()) valid = false;

    // Refresh performance cache after validation
    if (valid)
    {
        RefreshCache();
    }

    return valid;
}

bool PlayerbotConfig::ValidateBotLimits()
{
    bool valid = true;

    // Check bot limits
    uint32 maxBots = GetUInt("Playerbot.MaxBotsPerAccount", 10);
    if (maxBots == 0)
    {
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: Playerbot.MaxBotsPerAccount cannot be 0");
        valid = false;
    }
    else if (maxBots > 100)
    {
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: Playerbot.MaxBotsPerAccount ({}) exceeds maximum limit (100)", maxBots);
        valid = false;
    }
    else if (maxBots > 50)
    {
        TC_LOG_WARN("server.loading", "PlayerbotConfig: Playerbot.MaxBotsPerAccount ({}) exceeds recommended limit (50)", maxBots);
    }

    // Check global bot limits
    uint32 globalMaxBots = GetUInt("Playerbot.GlobalMaxBots", 1000);
    if (globalMaxBots < maxBots)
    {
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: Playerbot.GlobalMaxBots ({}) must be >= Playerbot.MaxBotsPerAccount ({})",
                     globalMaxBots, maxBots);
        valid = false;
    }

    return valid;
}

bool PlayerbotConfig::ValidateTimingSettings()
{
    bool valid = true;

    // Check update intervals
    uint32 updateInterval = GetUInt("Playerbot.UpdateInterval", 1000);
    if (updateInterval < 50)
    {
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: Playerbot.UpdateInterval ({}) is too low (minimum 50ms)", updateInterval);
        valid = false;
    }
    else if (updateInterval < 100)
    {
        TC_LOG_WARN("server.loading", "PlayerbotConfig: Playerbot.UpdateInterval ({}) is very low (recommended >=100ms)", updateInterval);
    }

    // Check AI decision time limits
    uint32 aiTimeLimit = GetUInt("Playerbot.AIDecisionTimeLimit", 50);
    if (aiTimeLimit == 0)
    {
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: Playerbot.AIDecisionTimeLimit cannot be 0");
        valid = false;
    }
    else if (aiTimeLimit > 1000)
    {
        TC_LOG_WARN("server.loading", "PlayerbotConfig: Playerbot.AIDecisionTimeLimit ({}) is very high (recommended <100ms)", aiTimeLimit);
    }

    // Check login delay settings
    uint32 loginDelay = GetUInt("Playerbot.LoginDelay", 1000);
    if (loginDelay > 60000)
    {
        TC_LOG_WARN("server.loading", "PlayerbotConfig: Playerbot.LoginDelay ({}) is very high (>60s)", loginDelay);
    }

    return valid;
}

bool PlayerbotConfig::ValidateLoggingSettings()
{
    bool valid = true;

    // Check log level
    uint32 logLevel = GetUInt("Playerbot.Log.Level", 4);
    if (logLevel > 6)
    {
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: Playerbot.Log.Level ({}) exceeds maximum (6)", logLevel);
        valid = false;
    }

    // Validate log file path
    std::string logFile = GetString("Playerbot.Log.File", "Playerbot.log");
    if (logFile.empty())
    {
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: Playerbot.Log.File cannot be empty");
        valid = false;
    }

    return valid;
}

bool PlayerbotConfig::ValidateDatabaseSettings()
{
    bool valid = true;

    // Check database connection timeout
    uint32 dbTimeout = GetUInt("Playerbot.Database.Timeout", 30);
    if (dbTimeout == 0)
    {
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: Playerbot.Database.Timeout cannot be 0");
        valid = false;
    }
    else if (dbTimeout > 300)
    {
        TC_LOG_WARN("server.loading", "PlayerbotConfig: Playerbot.Database.Timeout ({}) is very high (>5min)", dbTimeout);
    }

    return valid;
}

void PlayerbotConfig::InitializeLogging()
{
    // Check if ModuleLogManager singleton is available
    auto* mgr = sModuleLogManager;
    if (!mgr)
    {
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: ModuleLogManager singleton is NULL");
        return;
    }

    // Register Playerbot module with the new ModuleLogManager
    if (!mgr->RegisterModule("playerbot", 4, "Playerbot.log"))
    {
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: Failed to register module with ModuleLogManager");
        return;
    }

    // Apply Playerbot-specific configuration if loaded
    if (_loaded)
    {
        uint8 configLevel = GetUInt("Playerbot.Log.Level", 4);
        std::string configFile = GetString("Playerbot.Log.File", "Playerbot.log");

        if (configLevel <= 5)
        {
            mgr->SetModuleConfig("playerbot", configLevel, configFile);
        }
    }

    // Initialize module logging
    if (!mgr->InitializeModuleLogging("playerbot"))
    {
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: Failed to initialize module logging");
        return;
    }

    TC_LOG_INFO("server.loading", "PlayerbotConfig: New Module Logging system initialized successfully");

    // Test the new logging system with direct TC_LOG calls
    TC_LOG_INFO("module.playerbot", "Module logging system is now active");
    TC_LOG_ERROR("module.playerbot", "Error level test message");
}

void PlayerbotConfig::RefreshCache()
{
    std::lock_guard<std::mutex> lock(_configMutex);

    // Update cache with frequently accessed values
    _cache.maxBotsPerAccount = GetUInt("Playerbot.MaxBotsPerAccount", 10);
    _cache.globalMaxBots = GetUInt("Playerbot.GlobalMaxBots", 1000);
    _cache.updateInterval = GetUInt("Playerbot.UpdateInterval", 1000);
    _cache.aiDecisionTimeLimit = GetUInt("Playerbot.AIDecisionTimeLimit", 50);
    _cache.loginDelay = GetUInt("Playerbot.LoginDelay", 1000);
    _cache.logLevel = GetUInt("Playerbot.Log.Level", 4);
    _cache.logFile = GetString("Playerbot.Log.File", "Playerbot.log");
    _cache.databaseTimeout = GetUInt("Playerbot.Database.Timeout", 30);

    _cache.isValid = true;

    TC_LOG_DEBUG("server.loading", "PlayerbotConfig: Performance cache refreshed with {} values", 8);
}

// Template specializations for GetCached
template<>
uint32 PlayerbotConfig::GetCached(std::string const& key, uint32 defaultValue) const
{
    std::lock_guard<std::mutex> lock(_configMutex);
    _metrics.configLookups++;

    if (!_cache.isValid)
    {
        _metrics.cacheMisses++;
        // Fallback to normal lookup if cache is invalid
        return GetUInt(key, defaultValue);
    }

    // Fast cache lookup for frequently accessed values
    if (key == "Playerbot.MaxBotsPerAccount") { _metrics.cacheHits++; return _cache.maxBotsPerAccount; }
    if (key == "Playerbot.GlobalMaxBots") { _metrics.cacheHits++; return _cache.globalMaxBots; }
    if (key == "Playerbot.UpdateInterval") { _metrics.cacheHits++; return _cache.updateInterval; }
    if (key == "Playerbot.AIDecisionTimeLimit") { _metrics.cacheHits++; return _cache.aiDecisionTimeLimit; }
    if (key == "Playerbot.LoginDelay") { _metrics.cacheHits++; return _cache.loginDelay; }
    if (key == "Playerbot.Log.Level") { _metrics.cacheHits++; return _cache.logLevel; }
    if (key == "Playerbot.Database.Timeout") { _metrics.cacheHits++; return _cache.databaseTimeout; }

    // Fallback to normal lookup for non-cached values
    _metrics.cacheMisses++;
    return GetUInt(key, defaultValue);
}

template<>
std::string PlayerbotConfig::GetCached(std::string const& key, std::string defaultValue) const
{
    std::lock_guard<std::mutex> lock(_configMutex);

    if (!_cache.isValid)
    {
        return GetString(key, defaultValue);
    }

    if (key == "Playerbot.Log.File") return _cache.logFile;

    return GetString(key, defaultValue);
}

PlayerbotConfig::PerformanceMetrics PlayerbotConfig::GetPerformanceMetrics() const
{
    std::lock_guard<std::mutex> lock(_configMutex);
    return _metrics;
}

#endif // PLAYERBOT_ENABLED