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

#ifndef PLAYERBOT_CONFIG_H
#define PLAYERBOT_CONFIG_H

#include "Define.h"

/**
 * Configuration manager for the Playerbot module
 * Loads and manages all settings from playerbots.conf
 */
class TC_GAME_API PlayerbotConfig
{
public:
    /**
     * Load configuration from playerbots.conf
     * Called during server startup
     * @return true if configuration loaded successfully
     */
    static bool LoadConfig();
    
    /**
     * Reload configuration from playerbots.conf
     * Called when configuration needs to be refreshed
     */
    static void ReloadConfig();
    
    /**
     * Validate configuration values
     * Ensures all settings are within acceptable ranges
     * @return true if all settings are valid
     */
    static bool ValidateConfig();

    // Core Settings
    static bool IsEnabled() { return _enabled; }
    static uint32 GetMaxBots() { return _maxBots; }
    static uint32 GetMaxBotsPerAccount() { return _maxBotsPerAccount; }
    static uint32 GetUpdateInterval() { return _updateInterval; }
    static bool IsStartupValidationEnabled() { return _startupValidation; }

    // AI Behavior
    static uint32 GetAIUpdateDelay() { return _aiUpdateDelay; }
    static uint32 GetAICombatDelay() { return _aiCombatDelay; }
    static uint32 GetAIMovementDelay() { return _aiMovementDelay; }
    static uint32 GetAIThinkingDelay() { return _aiThinkingDelay; }
    static uint32 GetAIRandomFactor() { return _aiRandomFactor; }

    // Performance
    static bool IsPerformanceMonitoringEnabled() { return _performanceMonitoring; }
    static uint32 GetCPUWarningThreshold() { return _cpuWarningThreshold; }
    static uint32 GetMemoryWarningThreshold() { return _memoryWarningThreshold; }
    static float GetMaxCPUPerBot() { return _maxCPUPerBot; }
    static uint32 GetMaxMemoryPerBot() { return _maxMemoryPerBot; }

    // Database
    static bool IsDatabasePoolingEnabled() { return _databasePooling; }
    static uint32 GetMaxDatabaseConnections() { return _maxDatabaseConnections; }
    static uint32 GetDatabaseQueryTimeout() { return _databaseQueryTimeout; }
    static bool IsDatabaseCachingEnabled() { return _databaseCaching; }
    static uint32 GetDatabaseCacheTimeout() { return _databaseCacheTimeout; }

    // Naming System
    static bool IsRandomNamesEnabled() { return _randomNames; }
    static uint32 GetNamePoolSize() { return _namePoolSize; }
    static bool AreDuplicateNamesAllowed() { return _allowDuplicateNames; }
    static uint32 GetMinNameLength() { return _minNameLength; }
    static uint32 GetMaxNameLength() { return _maxNameLength; }

    // Social Features
    static bool IsGuildSystemEnabled() { return _enableGuilds; }
    static bool IsChatEnabled() { return _enableChat; }
    static uint32 GetChatChance() { return _chatChance; }
    static bool AreEmotesEnabled() { return _enableEmotes; }
    static uint32 GetEmoteChance() { return _emoteChance; }
    static bool IsGroupingEnabled() { return _enableGrouping; }
    static bool IsTradingEnabled() { return _enableTrading; }

    // Security
    static bool PreventBotLogin() { return _preventBotLogin; }
    static bool RestrictBotInteraction() { return _restrictBotInteraction; }
    static bool LogBotActions() { return _logBotActions; }
    static bool PreventBotExploit() { return _preventBotExploit; }
    static uint32 GetMaxGoldPerBot() { return _maxGoldPerBot; }

    // Experimental Features
    static bool IsRandomMovementEnabled() { return _experimentalRandomMovement; }
    static bool IsExperimentalChatEnabled() { return _experimentalBotChat; }
    static bool AreExperimentalEmotesEnabled() { return _experimentalBotEmotes; }
    static bool IsPathfindingV2Enabled() { return _experimentalPathfindingV2; }
    static bool AreAllExperimentalFeaturesEnabled() { return _experimentalFeatures; }

private:
    // Core Settings
    static bool _enabled;
    static uint32 _maxBots;
    static uint32 _maxBotsPerAccount;
    static uint32 _updateInterval;
    static bool _startupValidation;

    // AI Behavior
    static uint32 _aiUpdateDelay;
    static uint32 _aiCombatDelay;
    static uint32 _aiMovementDelay;
    static uint32 _aiThinkingDelay;
    static uint32 _aiRandomFactor;

    // Performance
    static bool _performanceMonitoring;
    static uint32 _cpuWarningThreshold;
    static uint32 _memoryWarningThreshold;
    static float _maxCPUPerBot;
    static uint32 _maxMemoryPerBot;

    // Database
    static bool _databasePooling;
    static uint32 _maxDatabaseConnections;
    static uint32 _databaseQueryTimeout;
    static bool _databaseCaching;
    static uint32 _databaseCacheTimeout;

    // Naming System
    static bool _randomNames;
    static uint32 _namePoolSize;
    static bool _allowDuplicateNames;
    static uint32 _minNameLength;
    static uint32 _maxNameLength;

    // Social Features
    static bool _enableGuilds;
    static bool _enableChat;
    static uint32 _chatChance;
    static bool _enableEmotes;
    static uint32 _emoteChance;
    static bool _enableGrouping;
    static bool _enableTrading;

    // Security
    static bool _preventBotLogin;
    static bool _restrictBotInteraction;
    static bool _logBotActions;
    static bool _preventBotExploit;
    static uint32 _maxGoldPerBot;

    // Experimental Features
    static bool _experimentalRandomMovement;
    static bool _experimentalBotChat;
    static bool _experimentalBotEmotes;
    static bool _experimentalPathfindingV2;
    static bool _experimentalFeatures;

    /**
     * Load a boolean configuration value with default
     * @param key Configuration key
     * @param defaultValue Default value if key not found
     * @return Configuration value
     */
    static bool LoadBoolConfig(const std::string& key, bool defaultValue);
    
    /**
     * Load a uint32 configuration value with default and range validation
     * @param key Configuration key
     * @param defaultValue Default value if key not found
     * @param minValue Minimum acceptable value
     * @param maxValue Maximum acceptable value
     * @return Configuration value (clamped to range)
     */
    static uint32 LoadUint32Config(const std::string& key, uint32 defaultValue, uint32 minValue, uint32 maxValue);
    
    /**
     * Load a float configuration value with default and range validation
     * @param key Configuration key
     * @param defaultValue Default value if key not found
     * @param minValue Minimum acceptable value
     * @param maxValue Maximum acceptable value
     * @return Configuration value (clamped to range)
     */
    static float LoadFloatConfig(const std::string& key, float defaultValue, float minValue, float maxValue);
};

#endif // PLAYERBOT_CONFIG_H
