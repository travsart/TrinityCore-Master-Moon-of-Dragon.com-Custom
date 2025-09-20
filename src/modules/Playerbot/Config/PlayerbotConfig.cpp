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
    printf("=== PLAYERBOT CONFIG DEBUG: Initialize() started ===\n");
    _loaded = false;
    _lastError.clear();

    // Find configuration file
    printf("=== PLAYERBOT CONFIG DEBUG: About to call FindConfigFile() ===\n");
    _configPath = FindConfigFile();
    printf("=== PLAYERBOT CONFIG DEBUG: FindConfigFile() returned, checking if empty ===\n");
    if (_configPath.empty())
    {
        _lastError = "Could not find playerbots.conf file";
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: {}", _lastError);
        printf("=== PLAYERBOT CONFIG DEBUG: Config file not found, returning false ===\n");
        return false;
    }

    printf("=== PLAYERBOT CONFIG DEBUG: Config file found, about to call LoadConfigFile() ===\n");
    // Load configuration
    if (!LoadConfigFile(_configPath))
    {
        _lastError = Trinity::StringFormat("Failed to load configuration from {}", _configPath);
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: {}", _lastError);
        return false;
    }

    printf("=== PLAYERBOT CONFIG DEBUG: LoadConfigFile completed, about to call ValidateConfiguration() ===\n");
    // Validate configuration
    if (!ValidateConfiguration())
    {
        _lastError = "Configuration validation failed";
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: {}", _lastError);
        return false;
    }

    printf("=== PLAYERBOT CONFIG DEBUG: ValidateConfiguration() succeeded, setting _loaded = true ===\n");
    _loaded = true;
    printf("=== PLAYERBOT CONFIG DEBUG: About to call TC_LOG_INFO ===\n");
    TC_LOG_INFO("server.loading", "PlayerbotConfig: Successfully loaded from {}", _configPath);
    printf("=== PLAYERBOT CONFIG DEBUG: Initialize() about to return true ===\n");
    return true;
}

bool PlayerbotConfig::Reload()
{
    if (_configPath.empty())
    {
        _lastError = "Configuration not initialized";
        return false;
    }

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
    auto it = _configValues.find(key);
    if (it == _configValues.end())
        return defaultValue;

    std::string value = it->second;
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    return (value == "1" || value == "true" || value == "yes" || value == "on");
}

int32 PlayerbotConfig::GetInt(std::string const& key, int32 defaultValue) const
{
    auto it = _configValues.find(key);
    if (it == _configValues.end())
        return defaultValue;

    try {
        return std::stoi(it->second);
    } catch (...) {
        return defaultValue;
    }
}

uint32 PlayerbotConfig::GetUInt(std::string const& key, uint32 defaultValue) const
{
    auto it = _configValues.find(key);
    if (it == _configValues.end())
        return defaultValue;

    try {
        return static_cast<uint32>(std::stoul(it->second));
    } catch (...) {
        return defaultValue;
    }
}

float PlayerbotConfig::GetFloat(std::string const& key, float defaultValue) const
{
    auto it = _configValues.find(key);
    if (it == _configValues.end())
        return defaultValue;

    try {
        return std::stof(it->second);
    } catch (...) {
        return defaultValue;
    }
}

std::string PlayerbotConfig::GetString(std::string const& key, std::string const& defaultValue) const
{
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
    printf("=== PLAYERBOT CONFIG DEBUG: LoadConfigFile() entered with path: %s ===\n", filePath.c_str());

    printf("=== PLAYERBOT CONFIG DEBUG: About to open ifstream ===\n");
    std::ifstream file(filePath);
    printf("=== PLAYERBOT CONFIG DEBUG: ifstream created, checking if open ===\n");

    if (!file.is_open())
    {
        printf("=== PLAYERBOT CONFIG DEBUG: File failed to open, returning false ===\n");
        return false;
    }

    printf("=== PLAYERBOT CONFIG DEBUG: File opened successfully, starting line reading loop ===\n");

    std::string line;
    int lineCount = 0;
    while (std::getline(file, line))
    {
        lineCount++;
        if (lineCount <= 5) // Log first 5 lines to avoid spam
        {
            printf("=== PLAYERBOT CONFIG DEBUG: Read line %d: %s ===\n", lineCount, line.c_str());
        }

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#')
            continue;

        // Find the = separator
        size_t pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        // Extract key and value
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        // Trim whitespace
        key.erase(key.find_last_not_of(" \t") + 1);
        key.erase(0, key.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));

        // Store the value
        _configValues[key] = value;

        if (lineCount <= 5) // Log first 5 config entries
        {
            printf("=== PLAYERBOT CONFIG DEBUG: Stored config [%s] = [%s] ===\n", key.c_str(), value.c_str());
        }
    }

    printf("=== PLAYERBOT CONFIG DEBUG: Finished reading %d lines, returning true ===\n", lineCount);
    return true;
}

bool PlayerbotConfig::ValidateConfiguration()
{
    printf("=== PLAYERBOT CONFIG DEBUG: ValidateConfiguration() entered ===\n");

    // Validate critical settings
    bool valid = true;

    // Check bot limits
    printf("=== PLAYERBOT CONFIG DEBUG: About to call GetUInt for MaxBotsPerAccount ===\n");
    uint32 maxBots = GetUInt("Playerbot.MaxBotsPerAccount", 10);
    printf("=== PLAYERBOT CONFIG DEBUG: GetUInt returned: %u ===\n", maxBots);
    if (maxBots > 50)
    {
        TC_LOG_WARN("server.loading", "PlayerbotConfig: Playerbot.MaxBotsPerAccount ({}) exceeds recommended limit (50)", maxBots);
    }

    // Check update intervals
    printf("=== PLAYERBOT CONFIG DEBUG: About to call GetUInt for UpdateInterval ===\n");
    uint32 updateInterval = GetUInt("Playerbot.UpdateInterval", 1000);
    printf("=== PLAYERBOT CONFIG DEBUG: GetUInt UpdateInterval returned: %u ===\n", updateInterval);
    if (updateInterval < 100)
    {
        TC_LOG_ERROR("server.loading", "PlayerbotConfig: Playerbot.UpdateInterval ({}) is too low (minimum 100ms)", updateInterval);
        valid = false;
    }

    // Check AI decision time limits
    uint32 aiTimeLimit = GetUInt("Playerbot.AIDecisionTimeLimit", 50);
    if (aiTimeLimit > 1000)
    {
        TC_LOG_WARN("server.loading", "PlayerbotConfig: Playerbot.AIDecisionTimeLimit ({}) is very high (recommended <100ms)", aiTimeLimit);
    }

    printf("=== PLAYERBOT CONFIG DEBUG: ValidateConfiguration() about to return: %s ===\n", valid ? "true" : "false");
    return valid;
}

void PlayerbotConfig::InitializeLogging()
{
    TC_LOG_DEBUG("server.loading", "PlayerbotConfig: Initializing logging system...");

    printf("=== PLAYERBOT DEBUG: Temporarily disabling custom logging setup to fix initialization ===\n");

    // Get log level from configuration
    int32 logLevel = GetInt("Playerbot.Log.Level", 3);
    std::string logFile = GetString("Playerbot.Log.File", "Playerbot.log");

    // Validate log level
    if (logLevel < 0 || logLevel > 5)
    {
        TC_LOG_WARN("server.loading", "PlayerbotConfig: Invalid log level {}. Using default level 3.", logLevel);
        logLevel = 3;
    }

    // TEMPORARILY DISABLED: Setup playerbot logging
    // SetupPlayerbotLogging(logLevel, logFile);
    printf("=== PLAYERBOT DEBUG: Skipped SetupPlayerbotLogging() call ===\n");

    TC_LOG_INFO("server.loading", "PlayerbotConfig: Logging system initialized - Level: {}, File: {} (custom logging disabled)", logLevel, logFile);
}

void PlayerbotConfig::SetupPlayerbotLogging(int32 logLevel, std::string const& logFile)
{
    // Create playerbot-specific file appender
    // Format: Type,Level,Flags,File
    // Type 2 = File appender, Flags 1 = Include timestamp
    std::string appenderConfig = Trinity::StringFormat("2,{},1,{}", logLevel, logFile);
    sLog->CreateAppenderFromConfigLine("Appender.Playerbot", appenderConfig);

    // Create main playerbot logger
    // Format: Level,Appender1 Appender2 ...
    std::string loggerConfig = Trinity::StringFormat("{},Console Playerbot", logLevel);
    sLog->CreateLoggerFromConfigLine("Logger.module.playerbot", loggerConfig);

    // Create specialized sub-loggers
    CreateSpecializedLoggers(logLevel);

    TC_LOG_DEBUG("server.loading", "PlayerbotConfig: Created logging appender and loggers");
}

void PlayerbotConfig::CreateSpecializedLoggers(int32 baseLevel)
{
    // AI decision logging
    if (GetBool("Playerbot.Log.AIDecisions", false))
    {
        int32 aiLogLevel = std::max(baseLevel, 4); // Force debug level for AI decisions
        std::string aiLoggerConfig = Trinity::StringFormat("{},Playerbot", aiLogLevel);
        sLog->CreateLoggerFromConfigLine("Logger.module.playerbot.ai", aiLoggerConfig);
        TC_LOG_DEBUG("server.loading", "PlayerbotConfig: Created AI decision logger (level {})", aiLogLevel);
    }

    // Performance metrics logging
    if (GetBool("Playerbot.Log.PerformanceMetrics", false))
    {
        std::string perfLoggerConfig = Trinity::StringFormat("{},Playerbot", baseLevel);
        sLog->CreateLoggerFromConfigLine("Logger.module.playerbot.performance", perfLoggerConfig);
        TC_LOG_DEBUG("server.loading", "PlayerbotConfig: Created performance metrics logger");
    }

    // Database query logging
    if (GetBool("Playerbot.Log.DatabaseQueries", false))
    {
        int32 dbLogLevel = std::max(baseLevel, 4); // Force debug level for database queries
        std::string dbLoggerConfig = Trinity::StringFormat("{},Playerbot", dbLogLevel);
        sLog->CreateLoggerFromConfigLine("Logger.module.playerbot.database", dbLoggerConfig);
        TC_LOG_DEBUG("server.loading", "PlayerbotConfig: Created database query logger (level {})", dbLogLevel);
    }

    // Character action logging
    if (GetBool("Playerbot.Log.CharacterActions", false))
    {
        int32 charLogLevel = std::max(baseLevel, 4); // Force debug level for character actions
        std::string charLoggerConfig = Trinity::StringFormat("{},Playerbot", charLogLevel);
        sLog->CreateLoggerFromConfigLine("Logger.module.playerbot.character", charLoggerConfig);
        TC_LOG_DEBUG("server.loading", "PlayerbotConfig: Created character action logger (level {})", charLogLevel);
    }

    // Account management logging
    std::string accountLoggerConfig = Trinity::StringFormat("{},Playerbot", baseLevel);
    sLog->CreateLoggerFromConfigLine("Logger.module.playerbot.account", accountLoggerConfig);

    // Name management logging
    std::string nameLoggerConfig = Trinity::StringFormat("{},Playerbot", baseLevel);
    sLog->CreateLoggerFromConfigLine("Logger.module.playerbot.names", nameLoggerConfig);

    TC_LOG_DEBUG("server.loading", "PlayerbotConfig: Created {} specialized loggers",
                 2 + (GetBool("Playerbot.Log.AIDecisions", false) ? 1 : 0) +
                     (GetBool("Playerbot.Log.PerformanceMetrics", false) ? 1 : 0) +
                     (GetBool("Playerbot.Log.DatabaseQueries", false) ? 1 : 0) +
                     (GetBool("Playerbot.Log.CharacterActions", false) ? 1 : 0));
}

#endif // PLAYERBOT_ENABLED