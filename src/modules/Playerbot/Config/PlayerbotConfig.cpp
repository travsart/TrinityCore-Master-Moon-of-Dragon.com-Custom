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
    std::ifstream file(filePath);
    if (!file.is_open())
        return false;

    std::string line;
    while (std::getline(file, line))
    {
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
    }

    return true;
}

bool PlayerbotConfig::ValidateConfiguration()
{

    // Validate critical settings
    bool valid = true;

    // Check bot limits
    uint32 maxBots = GetUInt("Playerbot.MaxBotsPerAccount", 10);
    if (maxBots > 50)
    {
        TC_LOG_WARN("server.loading", "PlayerbotConfig: Playerbot.MaxBotsPerAccount ({}) exceeds recommended limit (50)", maxBots);
    }

    // Check update intervals
    uint32 updateInterval = GetUInt("Playerbot.UpdateInterval", 1000);
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

    return valid;
}

#endif // PLAYERBOT_ENABLED