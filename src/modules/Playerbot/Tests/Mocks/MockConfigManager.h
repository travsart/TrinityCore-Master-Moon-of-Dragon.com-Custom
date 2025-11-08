/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 */

#pragma once

#include "Core/DI/Interfaces/IConfigManager.h"
#include <unordered_map>
#include <string>

namespace Playerbot::Testing
{

/**
 * @brief Mock implementation of IConfigManager for unit testing
 *
 * Provides a simple in-memory configuration store for testing
 * without file I/O or complex validation logic.
 */
class MockConfigManager final : public IConfigManager
{
public:
    MockConfigManager() : _initialized(false) {}

    // IConfigManager interface implementation
    bool Initialize() override
    {
        _initialized = true;
        _config.clear();
        _callCounts.clear();
        return true;
    }

    bool SetValue(std::string const& key, ConfigValue const& value) override
    {
        _config[key] = value;
        _callCounts["SetValue"]++;
        return true;
    }

    bool GetBool(std::string const& key, bool defaultValue) const override
    {
        _callCounts["GetBool"]++;
        auto it = _config.find(key);
        if (it != _config.end() && std::holds_alternative<bool>(it->second))
            return std::get<bool>(it->second);
        return defaultValue;
    }

    int32 GetInt(std::string const& key, int32 defaultValue) const override
    {
        _callCounts["GetInt"]++;
        auto it = _config.find(key);
        if (it != _config.end() && std::holds_alternative<int32>(it->second))
            return std::get<int32>(it->second);
        return defaultValue;
    }

    uint32 GetUInt(std::string const& key, uint32 defaultValue) const override
    {
        _callCounts["GetUInt"]++;
        auto it = _config.find(key);
        if (it != _config.end() && std::holds_alternative<uint32>(it->second))
            return std::get<uint32>(it->second);
        return defaultValue;
    }

    float GetFloat(std::string const& key, float defaultValue) const override
    {
        _callCounts["GetFloat"]++;
        auto it = _config.find(key);
        if (it != _config.end() && std::holds_alternative<float>(it->second))
            return std::get<float>(it->second);
        return defaultValue;
    }

    std::string GetString(std::string const& key, std::string const& defaultValue) const override
    {
        _callCounts["GetString"]++;
        auto it = _config.find(key);
        if (it != _config.end() && std::holds_alternative<std::string>(it->second))
            return std::get<std::string>(it->second);
        return defaultValue;
    }

    void RegisterCallback(std::string const& key, ChangeCallback callback) override
    {
        _callbacks[key].push_back(callback);
        _callCounts["RegisterCallback"]++;
    }

    std::map<std::string, ConfigEntry> GetAllEntries() const override
    {
        _callCounts["GetAllEntries"]++;
        std::map<std::string, ConfigEntry> entries;
        for (auto const& [key, value] : _config)
        {
            ConfigEntry entry;
            entry.value = value;
            entry.defaultValue = value;
            entry.description = "Test config";
            entry.persistent = false;
            entry.readOnly = false;
            entries[key] = entry;
        }
        return entries;
    }

    void ResetToDefaults() override
    {
        _config.clear();
        _callCounts["ResetToDefaults"]++;
    }

    bool SaveToFile(std::string const& /*filePath*/) const override
    {
        _callCounts["SaveToFile"]++;
        return true; // Simulate successful save
    }

    bool LoadFromFile(std::string const& /*filePath*/) override
    {
        _callCounts["LoadFromFile"]++;
        return true; // Simulate successful load
    }

    std::string GetLastError() const override
    {
        return _lastError;
    }

    bool HasKey(std::string const& key) const override
    {
        _callCounts["HasKey"]++;
        return _config.find(key) != _config.end();
    }

    std::optional<ConfigEntry> GetEntry(std::string const& key) const override
    {
        _callCounts["GetEntry"]++;
        auto it = _config.find(key);
        if (it != _config.end())
        {
            ConfigEntry entry;
            entry.value = it->second;
            entry.defaultValue = it->second;
            entry.description = "Test config";
            entry.persistent = false;
            entry.readOnly = false;
            return entry;
        }
        return std::nullopt;
    }

    // Mock-specific verification methods
    uint32 GetCallCount(std::string const& methodName) const
    {
        auto it = _callCounts.find(methodName);
        return it != _callCounts.end() ? it->second : 0;
    }

    void ClearCallCounts()
    {
        _callCounts.clear();
    }

    bool WasMethodCalled(std::string const& methodName) const
    {
        return GetCallCount(methodName) > 0;
    }

private:
    mutable std::unordered_map<std::string, uint32> _callCounts;
    std::unordered_map<std::string, ConfigValue> _config;
    std::unordered_map<std::string, std::vector<ChangeCallback>> _callbacks;
    std::string _lastError;
    bool _initialized;
};

} // namespace Playerbot::Testing
