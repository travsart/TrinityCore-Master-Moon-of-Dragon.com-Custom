/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "BotNameMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "CharacterCache.h"
#include "PlayerbotDatabase.h"
#include "PreparedStatement.h"
#include <random>
#include <algorithm>

BotNameMgr* BotNameMgr::instance()
{
    static BotNameMgr instance;
    return &instance;
}

bool BotNameMgr::Initialize()
{
    LoadNamesFromDatabase();
    LoadUsedNames();
    
    TC_LOG_INFO("module.playerbot.names", 
        "Loaded {} names ({} male, {} female), {} in use", 
        _names.size(),
        _availableMaleNames.size() + (_guidToNameId.size() / 2), // Approximate
        _availableFemaleNames.size() + (_guidToNameId.size() / 2),
        _guidToNameId.size());
    
    return true;
}

void BotNameMgr::Shutdown()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _names.clear();
    _availableMaleNames.clear();
    _availableFemaleNames.clear();
    _nameToId.clear();
    _guidToNameId.clear();
    _nameIdToGuid.clear();
}

std::string BotNameMgr::AllocateName(uint8 gender, uint32 characterGuid)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    // Check if character already has a name
    auto it = _guidToNameId.find(characterGuid);
    if (it != _guidToNameId.end())
    {
        auto nameIt = _names.find(it->second);
        if (nameIt != _names.end())
        {
            TC_LOG_WARN("module.playerbot.names", 
                "Character {} already has name '{}'", 
                characterGuid, nameIt->second.name);
            return nameIt->second.name;
        }
    }
    
    // Get available names for gender
    std::unordered_set<uint32>* availableNames = nullptr;
    if (gender == 0) // Male
        availableNames = &_availableMaleNames;
    else if (gender == 1) // Female
        availableNames = &_availableFemaleNames;
    else
    {
        TC_LOG_ERROR("module.playerbot.names", 
            "Invalid gender {} for name allocation", gender);
        return "";
    }
    
    if (availableNames->empty())
    {
        TC_LOG_ERROR("module.playerbot.names", 
            "No available names for gender {}", gender);
        return "";
    }
    
    // Pick a random available name
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(0, availableNames->size() - 1);
    
    auto iter = availableNames->begin();
    std::advance(iter, dis(gen));
    uint32 selectedNameId = *iter;
    
    // Get the name entry
    auto nameIt = _names.find(selectedNameId);
    if (nameIt == _names.end())
    {
        TC_LOG_ERROR("module.playerbot.names", 
            "Name ID {} not found in names map", selectedNameId);
        return "";
    }
    
    NameEntry& entry = nameIt->second;
    std::string allocatedName = entry.name;
    
    // Mark as used
    entry.used = true;
    entry.usedByGuid = characterGuid;
    
    // Update available names set
    availableNames->erase(selectedNameId);
    
    // Update mappings
    _guidToNameId[characterGuid] = selectedNameId;
    _nameIdToGuid[selectedNameId] = characterGuid;
    
    // Update database
    PreparedStatement<PlayerbotDatabaseConnection>* stmt = PlayerbotDatabase.GetPreparedStatement(PLAYERBOT_INS_NAME_USED);
    stmt->setUInt32(0, selectedNameId);
    stmt->setUInt32(1, characterGuid);
    PlayerbotDatabase.Execute(stmt);
    
    TC_LOG_INFO("module.playerbot.names", 
        "Allocated name '{}' (ID: {}) to character {}", 
        allocatedName, selectedNameId, characterGuid);
    
    return allocatedName;
}

void BotNameMgr::ReleaseName(uint32 characterGuid)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto it = _guidToNameId.find(characterGuid);
    if (it == _guidToNameId.end())
    {
        TC_LOG_DEBUG("module.playerbot.names", 
            "Character {} has no allocated name", characterGuid);
        return;
    }
    
    uint32 nameId = it->second;
    
    // Find the name entry
    auto nameIt = _names.find(nameId);
    if (nameIt != _names.end())
    {
        NameEntry& entry = nameIt->second;
        std::string releasedName = entry.name;
        
        // Mark as available
        entry.used = false;
        entry.usedByGuid = 0;
        
        // Add back to available names
        if (entry.gender == 0)
            _availableMaleNames.insert(nameId);
        else if (entry.gender == 1)
            _availableFemaleNames.insert(nameId);
        
        TC_LOG_INFO("module.playerbot.names", 
            "Released name '{}' (ID: {}) from character {}", 
            releasedName, nameId, characterGuid);
    }
    
    // Remove from mappings
    _guidToNameId.erase(characterGuid);
    _nameIdToGuid.erase(nameId);
    
    // Update database
    PreparedStatement<PlayerbotDatabaseConnection>* stmt = PlayerbotDatabase.GetPreparedStatement(PLAYERBOT_DEL_NAME_USED);
    stmt->setUInt32(0, nameId);
    PlayerbotDatabase.Execute(stmt);
}

void BotNameMgr::ReleaseName(std::string const& name)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto it = _nameToId.find(name);
    if (it == _nameToId.end())
    {
        TC_LOG_WARN("module.playerbot.names", 
            "Name '{}' not found in name pool", name);
        return;
    }
    
    uint32 nameId = it->second;
    auto guidIt = _nameIdToGuid.find(nameId);
    if (guidIt != _nameIdToGuid.end())
    {
        ReleaseName(guidIt->second);
    }
}

bool BotNameMgr::IsNameAvailable(std::string const& name) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto it = _nameToId.find(name);
    if (it == _nameToId.end())
        return false; // Name not in pool
    
    auto nameIt = _names.find(it->second);
    if (nameIt == _names.end())
        return false;
    
    return !nameIt->second.used;
}

std::string BotNameMgr::GetCharacterName(uint32 characterGuid) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto it = _guidToNameId.find(characterGuid);
    if (it == _guidToNameId.end())
        return "";
    
    auto nameIt = _names.find(it->second);
    if (nameIt == _names.end())
        return "";
    
    return nameIt->second.name;
}

uint32 BotNameMgr::GetAvailableNameCount(uint8 gender) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    if (gender == 0)
        return _availableMaleNames.size();
    else if (gender == 1)
        return _availableFemaleNames.size();
    else
        return _availableMaleNames.size() + _availableFemaleNames.size();
}

uint32 BotNameMgr::GetTotalNameCount() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _names.size();
}

uint32 BotNameMgr::GetUsedNameCount() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _guidToNameId.size();
}

void BotNameMgr::ReloadNames()
{
    TC_LOG_INFO("module.playerbot.names", "Reloading name pool from database");
    
    std::lock_guard<std::mutex> lock(_mutex);
    
    // Clear current data
    _names.clear();
    _availableMaleNames.clear();
    _availableFemaleNames.clear();
    _nameToId.clear();
    
    // Reload
    LoadNamesFromDatabase();
    LoadUsedNames();
}

void BotNameMgr::LoadNamesFromDatabase()
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT name_id, name, gender FROM playerbots_names");
    
    if (!result)
    {
        TC_LOG_ERROR("module.playerbot.names", 
            "No names found in playerbots_names table!");
        return;
    }
    
    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        
        NameEntry entry;
        entry.nameId = fields[0].GetUInt32();
        entry.name = fields[1].GetString();
        entry.gender = fields[2].GetUInt8();
        entry.used = false;
        entry.usedByGuid = 0;
        
        _names[entry.nameId] = entry;
        _nameToId[entry.name] = entry.nameId;
        
        // Add to available names (will be adjusted when loading used names)
        if (entry.gender == 0)
            _availableMaleNames.insert(entry.nameId);
        else if (entry.gender == 1)
            _availableFemaleNames.insert(entry.nameId);
        
        ++count;
        
    } while (result->NextRow());
    
    TC_LOG_INFO("module.playerbot.names", 
        "Loaded {} names from database", count);
}

void BotNameMgr::LoadUsedNames()
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT name_id, character_guid FROM playerbots_names_used");
    
    if (!result)
    {
        TC_LOG_DEBUG("module.playerbot.names", "No used names found");
        return;
    }
    
    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 nameId = fields[0].GetUInt32();
        uint32 characterGuid = fields[1].GetUInt32();
        
        auto it = _names.find(nameId);
        if (it == _names.end())
        {
            TC_LOG_WARN("module.playerbot.names", 
                "Used name ID {} not found in name pool", nameId);
            continue;
        }
        
        NameEntry& entry = it->second;
        entry.used = true;
        entry.usedByGuid = characterGuid;
        
        // Remove from available names
        if (entry.gender == 0)
            _availableMaleNames.erase(nameId);
        else if (entry.gender == 1)
            _availableFemaleNames.erase(nameId);
        
        // Update mappings
        _guidToNameId[characterGuid] = nameId;
        _nameIdToGuid[nameId] = characterGuid;
        
        ++count;
        
    } while (result->NextRow());
    
    TC_LOG_INFO("module.playerbot.names", 
        "Loaded {} used names", count);
}
