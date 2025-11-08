/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef BOT_NAME_MGR_H
#define BOT_NAME_MGR_H

#include "Define.h"
#include "Threading/LockHierarchy.h"
#include "Core/DI/Interfaces/IBotNameMgr.h"
#include <string>
#include <mutex>
#include <unordered_set>
#include <unordered_map>

/**
 * @brief Bot Name Manager
 *
 * Implements IBotNameMgr for dependency injection compatibility.
 * Manages allocation and tracking of bot character names.
 */
class TC_GAME_API BotNameMgr final : public IBotNameMgr
{
public:
    BotNameMgr(BotNameMgr const&) = delete;
    BotNameMgr& operator=(BotNameMgr const&) = delete;

    static BotNameMgr* instance();

    // IBotNameMgr interface implementation
    bool Initialize() override;
    void Shutdown() override;

    // Name allocation - returns empty string if no name available
    std::string AllocateName(uint8 gender, uint32 characterGuid) override;

    // Name release when character is deleted
    void ReleaseName(uint32 characterGuid) override;
    void ReleaseName(std::string const& name) override;

    // Check if name is available
    bool IsNameAvailable(std::string const& name) const override;

    // Get name for existing character
    std::string GetCharacterName(uint32 characterGuid) const override;

    // Statistics
    uint32 GetAvailableNameCount(uint8 gender) const override;
    uint32 GetTotalNameCount() const override;
    uint32 GetUsedNameCount() const override;

    // Reload names from database
    void ReloadNames() override;
    
private:
    BotNameMgr() = default;
    ~BotNameMgr() = default;
    
    void LoadNamesFromDatabase();
    void LoadUsedNames();
    
    struct NameEntry
    {
        uint32 nameId;
        std::string name;
        uint8 gender;
        bool used;
        uint32 usedByGuid;
    };
    
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BEHAVIOR_MANAGER> _mutex;
    
    // All names indexed by ID
    std::unordered_map<uint32, NameEntry> _names;
    
    // Available names by gender (name_id sets)
    std::unordered_set<uint32> _availableMaleNames;
    std::unordered_set<uint32> _availableFemaleNames;
    
    // Used names mapping
    std::unordered_map<std::string, uint32> _nameToId;        // name -> name_id
    std::unordered_map<uint32, uint32> _guidToNameId;         // character_guid -> name_id
    std::unordered_map<uint32, uint32> _nameIdToGuid;         // name_id -> character_guid
};

#define sBotNameMgr BotNameMgr::instance()

#endif // BOT_NAME_MGR_H
