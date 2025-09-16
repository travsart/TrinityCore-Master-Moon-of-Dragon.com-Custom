/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef BOT_ACCOUNT_MGR_H
#define BOT_ACCOUNT_MGR_H

#include "Define.h"
#include "SharedDefines.h"
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <string>
#include <vector>

struct BotAccountInfo
{
    uint32 accountId;
    std::string username;
    bool isActive;
    uint32 characterCount;
    time_t lastLogin;
};

class TC_GAME_API BotAccountMgr
{
    BotAccountMgr(BotAccountMgr const&) = delete;
    BotAccountMgr& operator=(BotAccountMgr const&) = delete;

public:
    static BotAccountMgr* instance();
    
    // Character limit constants
    static constexpr uint32 MAX_CHARACTERS_PER_BOT_ACCOUNT = 10;
    
    // Initialization
    bool Initialize();
    void Shutdown();
    
    // Account management
    uint32 CreateBotAccount(std::string const& username);
    bool DeleteBotAccount(uint32 accountId);
    bool IsBotAccount(uint32 accountId) const;
    std::vector<BotAccountInfo> GetAllBotAccounts() const;
    
    // Character limit management
    bool CanCreateCharacter(uint32 accountId) const;
    uint32 GetCharacterCount(uint32 accountId) const;
    bool ValidateCharacterCount(uint32 accountId);
    bool ValidateAllBotAccounts();
    bool ValidateCharacterLimitsOnStartup();
    void AutoFixCharacterLimitViolations();
    
    // Hooks for character creation/deletion
    void OnCharacterCreate(uint32 accountId, uint32 characterGuid);
    void OnCharacterDelete(uint32 accountId, uint32 characterGuid);
    
    // Statistics
    uint32 GetTotalBotAccounts() const;
    uint32 GetActiveBotAccounts() const;
    
private:
    BotAccountMgr() = default;
    ~BotAccountMgr() = default;
    
    void LoadBotAccounts();
    void LoadCharacterCounts();
    void UpdateCharacterCount(uint32 accountId);
    void DisableExcessCharacters(uint32 accountId, uint32 excessCount);
    
    // Data storage
    std::unordered_map<uint32, BotAccountInfo> _botAccounts;
    std::unordered_map<uint32, uint32> _characterCounts;
    std::unordered_set<uint32> _botAccountIds;
    
    // Thread safety
    mutable std::mutex _accountMutex;
    mutable std::mutex _characterCountMutex;
    
    // Statistics
    uint32 _totalAccounts = 0;
    uint32 _activeAccounts = 0;
    uint32 _nextAccountId = 0;
};

#define sBotAccountMgr BotAccountMgr::instance()

#endif // BOT_ACCOUNT_MGR_H
