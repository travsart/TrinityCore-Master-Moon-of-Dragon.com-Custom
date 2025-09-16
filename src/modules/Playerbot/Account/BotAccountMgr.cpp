/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "BotAccountMgr.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "World.h"
#include "AccountMgr.h"
#include "Random.h"
#include "PlayerbotDatabase.h"
#include "PreparedStatement.h"
#include <sstream>
#include <cstdlib>

BotAccountMgr* BotAccountMgr::instance()
{
    static BotAccountMgr instance;
    return &instance;
}

bool BotAccountMgr::Initialize()
{
    TC_LOG_INFO("module.playerbot", "Initializing Bot Account Manager...");
    
    // Load existing bot accounts
    LoadBotAccounts();
    
    // Load character counts
    LoadCharacterCounts();
    
    // Validate character limits on startup
    if (!ValidateCharacterLimitsOnStartup())
    {
        if (sConfigMgr->GetBoolDefault("Playerbot.StrictCharacterLimit", true))
        {
            TC_LOG_ERROR("module.playerbot", 
                "Bot account character limit validation failed! "
                "Some bot accounts have more than {} characters", 
                MAX_CHARACTERS_PER_BOT_ACCOUNT);
            return false;
        }
    }
    
    TC_LOG_INFO("module.playerbot", 
        "Bot Account Manager initialized with {} accounts", _totalAccounts);
    
    return true;
}

void BotAccountMgr::Shutdown()
{
    std::lock_guard<std::mutex> lockAccounts(_accountMutex);
    std::lock_guard<std::mutex> lockCounts(_characterCountMutex);
    
    _botAccounts.clear();
    _characterCounts.clear();
    _botAccountIds.clear();
    
    TC_LOG_INFO("module.playerbot", "Bot Account Manager shut down");
}

void BotAccountMgr::LoadBotAccounts()
{
    std::lock_guard<std::mutex> lock(_accountMutex);
    
    _botAccounts.clear();
    _botAccountIds.clear();
    _totalAccounts = 0;
    _activeAccounts = 0;
    
    // Query bot accounts from database
    QueryResult result = LoginDatabase.Query(
        "SELECT a.id, a.username, pa.is_active, pa.character_count, a.last_login "
        "FROM account a "
        "INNER JOIN playerbot_accounts pa ON a.id = pa.account_id "
        "WHERE pa.is_bot = 1");
    
    if (!result)
    {
        TC_LOG_INFO("module.playerbot", "No bot accounts found in database");
        return;
    }
    
    do
    {
        Field* fields = result->Fetch();
        
        BotAccountInfo info;
        info.accountId = fields[0].GetUInt32();
        info.username = fields[1].GetString();
        info.isActive = fields[2].GetBool();
        info.characterCount = fields[3].GetUInt32();
        info.lastLogin = fields[4].GetUInt32();
        
        _botAccounts[info.accountId] = info;
        _botAccountIds.insert(info.accountId);
        
        _totalAccounts++;
        if (info.isActive)
            _activeAccounts++;
        
        if (info.accountId >= _nextAccountId)
            _nextAccountId = info.accountId + 1;
            
    } while (result->NextRow());
    
    TC_LOG_INFO("module.playerbot", 
        "Loaded {} bot accounts ({} active)", 
        _totalAccounts, _activeAccounts);
}

void BotAccountMgr::LoadCharacterCounts()
{
    std::lock_guard<std::mutex> lock(_characterCountMutex);
    _characterCounts.clear();
    
    QueryResult result = PlayerbotDatabase.Query(
        "SELECT account, COUNT(*) as char_count "
        "FROM characters "
        "WHERE account IN (SELECT account_id FROM playerbot_accounts WHERE is_bot = 1) "
        "GROUP BY account");
    
    if (!result)
    {
        TC_LOG_DEBUG("module.playerbot", "No bot characters found");
        return;
    }
    
    uint32 violations = 0;
    
    do
    {
        Field* fields = result->Fetch();
        uint32 accountId = fields[0].GetUInt32();
        uint32 count = fields[1].GetUInt32();
        
        _characterCounts[accountId] = count;
        
        if (count > MAX_CHARACTERS_PER_BOT_ACCOUNT)
        {
            TC_LOG_WARN("module.playerbot", 
                "Bot account {} has {} characters (max: {})",
                accountId, count, MAX_CHARACTERS_PER_BOT_ACCOUNT);
            violations++;
        }
    } while (result->NextRow());
    
    TC_LOG_INFO("module.playerbot", 
        "Loaded character counts for {} bot accounts ({} violations)",
        _characterCounts.size(), violations);
}

bool BotAccountMgr::IsBotAccount(uint32 accountId) const
{
    std::lock_guard<std::mutex> lock(_accountMutex);
    return _botAccountIds.find(accountId) != _botAccountIds.end();
}

bool BotAccountMgr::CanCreateCharacter(uint32 accountId) const
{
    // Check if it's a bot account
    if (!IsBotAccount(accountId))
    {
        // Not a bot account, no special limit
        return true;
    }
    
    uint32 count = GetCharacterCount(accountId);
    
    if (count >= MAX_CHARACTERS_PER_BOT_ACCOUNT)
    {
        TC_LOG_WARN("module.playerbot", 
            "Bot account {} already has {} characters (max: {})",
            accountId, count, MAX_CHARACTERS_PER_BOT_ACCOUNT);
        return false;
    }
    
    return true;
}

uint32 BotAccountMgr::GetCharacterCount(uint32 accountId) const
{
    std::lock_guard<std::mutex> lock(_characterCountMutex);
    
    auto it = _characterCounts.find(accountId);
    if (it != _characterCounts.end())
    {
        return it->second;
    }
    
    // Not in cache, query database
    PreparedStatement<PlayerbotDatabaseConnection>* stmt = PlayerbotDatabase.GetPreparedStatement(PLAYERBOT_SEL_ACCOUNT_CHARACTER_COUNT);
    stmt->setUInt32(0, accountId);
    PreparedQueryResult result = PlayerbotDatabase.Query(stmt);
    
    if (result)
    {
        Field* fields = result->Fetch();
        uint32 count = fields[0].GetUInt32();
        
        // Update cache
        const_cast<BotAccountMgr*>(this)->_characterCounts[accountId] = count;
        return count;
    }
    
    return 0;
}

bool BotAccountMgr::ValidateCharacterLimitsOnStartup()
{
    TC_LOG_INFO("module.playerbot", 
        "Validating bot account character limits on startup...");
    
    // Check all bot accounts
    QueryResult result = PlayerbotDatabase.Query(
        "SELECT c.account, COUNT(*) as char_count "
        "FROM characters c "
        "INNER JOIN playerbot_accounts pa ON c.account = pa.account_id "
        "WHERE pa.is_bot = 1 "
        "GROUP BY c.account "
        "HAVING char_count > " + std::to_string(MAX_CHARACTERS_PER_BOT_ACCOUNT));
    
    if (!result)
    {
        TC_LOG_INFO("module.playerbot", 
            "All bot accounts are within character limit");
        return true;
    }
    
    uint32 violationCount = 0;
    std::stringstream violations;
    
    do
    {
        Field* fields = result->Fetch();
        uint32 accountId = fields[0].GetUInt32();
        uint32 charCount = fields[1].GetUInt32();
        
        violations << "Account " << accountId << ": " << charCount << " characters; ";
        violationCount++;
        
        TC_LOG_ERROR("module.playerbot", 
            "Bot account {} has {} characters (limit: {})",
            accountId, charCount, MAX_CHARACTERS_PER_BOT_ACCOUNT);
        
    } while (result->NextRow());
    
    if (violationCount > 0)
    {
        TC_LOG_ERROR("module.playerbot", 
            "{} bot accounts exceed character limit: {}",
            violationCount, violations.str());
        
        // Depending on configuration, either fail startup or auto-fix
        if (sConfigMgr->GetBoolDefault("Playerbot.StrictCharacterLimit", true))
        {
            TC_LOG_FATAL("module.playerbot", 
                "Startup aborted due to bot account character limit violations. "
                "Set Playerbot.StrictCharacterLimit = 0 to allow startup with violations.");
            return false;
        }
        else
        {
            TC_LOG_WARN("module.playerbot", 
                "Continuing startup despite character limit violations. "
                "Excess characters will be disabled.");
            
            AutoFixCharacterLimitViolations();
        }
    }
    
    return true;
}

void BotAccountMgr::DisableExcessCharacters(uint32 accountId, uint32 excessCount)
{
    TC_LOG_WARN("module.playerbot", 
        "Disabling {} excess characters for bot account {}",
        excessCount, accountId);
    
    // Get characters ordered by level (lowest first) and play time
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARS_BY_ACCOUNT_ID);
    stmt->setUInt32(0, accountId);
    PreparedQueryResult result = PlayerbotDatabase.Query(stmt);
    
    if (!result)
        return;
    
    std::vector<uint32> charactersToDisable;
    uint32 count = 0;
    
    // Skip the first MAX_CHARACTERS_PER_BOT_ACCOUNT characters
    for (uint32 i = 0; i < MAX_CHARACTERS_PER_BOT_ACCOUNT && result->NextRow(); ++i);
    
    // Collect excess characters
    do
    {
        Field* fields = result->Fetch();
        uint32 guid = fields[0].GetUInt32();
        charactersToDisable.push_back(guid);
        
        if (++count >= excessCount)
            break;
            
    } while (result->NextRow());
    
    // Mark characters as disabled
    for (uint32 guid : charactersToDisable)
    {
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ADD_AT_LOGIN_FLAG);
        stmt->setUInt16(0, uint16(0x00000001 | 0x00000008)); // AT_LOGIN_RENAME | AT_LOGIN_CUSTOMIZE equivalent
        stmt->setUInt32(1, guid);
        PlayerbotDatabase.Execute(stmt);
        
        TC_LOG_WARN("module.playerbot", 
            "Disabled excess character {} from bot account {}",
            guid, accountId);
    }
}

void BotAccountMgr::AutoFixCharacterLimitViolations()
{
    TC_LOG_INFO("module.playerbot", "Auto-fixing character limit violations...");
    
    QueryResult result = PlayerbotDatabase.Query(
        "SELECT c.account, COUNT(*) as char_count "
        "FROM characters c "
        "INNER JOIN playerbot_accounts pa ON c.account = pa.account_id "
        "WHERE pa.is_bot = 1 "
        "GROUP BY c.account "
        "HAVING char_count > " + std::to_string(MAX_CHARACTERS_PER_BOT_ACCOUNT));
    
    if (!result)
        return;
    
    uint32 fixedCount = 0;
    
    do
    {
        Field* fields = result->Fetch();
        uint32 accountId = fields[0].GetUInt32();
        uint32 charCount = fields[1].GetUInt32();
        uint32 excess = charCount - MAX_CHARACTERS_PER_BOT_ACCOUNT;
        
        DisableExcessCharacters(accountId, excess);
        fixedCount++;
        
    } while (result->NextRow());
    
    TC_LOG_INFO("module.playerbot", 
        "Fixed {} bot accounts with character limit violations",
        fixedCount);
}

void BotAccountMgr::OnCharacterCreate(uint32 accountId, uint32 characterGuid)
{
    if (!IsBotAccount(accountId))
        return;
    
    std::lock_guard<std::mutex> lock(_characterCountMutex);
    _characterCounts[accountId]++;
    
    UpdateCharacterCount(accountId);
    
    TC_LOG_DEBUG("module.playerbot", 
        "Bot account {} now has {} characters",
        accountId, _characterCounts[accountId]);
}

void BotAccountMgr::OnCharacterDelete(uint32 accountId, uint32 characterGuid)
{
    if (!IsBotAccount(accountId))
        return;
    
    std::lock_guard<std::mutex> lock(_characterCountMutex);
    
    if (_characterCounts[accountId] > 0)
    {
        _characterCounts[accountId]--;
        UpdateCharacterCount(accountId);
        
        TC_LOG_DEBUG("module.playerbot", 
            "Bot account {} now has {} characters",
            accountId, _characterCounts[accountId]);
    }
}

void BotAccountMgr::UpdateCharacterCount(uint32 accountId)
{
    uint32 count = GetCharacterCount(accountId);
    
    PreparedStatement<PlayerbotDatabaseConnection>* stmt = PlayerbotDatabase.GetPreparedStatement(PLAYERBOT_UPD_BOT_ACCOUNT_CHARACTER_COUNT);
    stmt->setUInt32(0, count);
    stmt->setUInt32(1, accountId);
    PlayerbotDatabase.Execute(stmt);
    
    // Update cache
    std::lock_guard<std::mutex> lock(_accountMutex);
    auto it = _botAccounts.find(accountId);
    if (it != _botAccounts.end())
    {
        it->second.characterCount = count;
    }
}

uint32 BotAccountMgr::CreateBotAccount(std::string const& username)
{
    // Generate a secure password
    std::string password = "BotPass" + std::to_string(urand(10000, 99999));
    
    // Create account using AccountMgr
    AccountOpResult result = AccountMgr().CreateAccount(username, password, "");
    
    if (result != AccountOpResult::AOR_OK)
    {
        TC_LOG_ERROR("module.playerbot", 
            "Failed to create bot account '{}': {}", 
            username, uint32(result));
        return 0;
    }
    
    // Get the created account ID
    uint32 accountId = AccountMgr::GetId(username);
    if (!accountId)
    {
        TC_LOG_ERROR("module.playerbot", 
            "Failed to get ID for newly created bot account '{}'", username);
        return 0;
    }
    
    // Mark as bot account in our table
    PreparedStatement<PlayerbotDatabaseConnection>* stmt = PlayerbotDatabase.GetPreparedStatement(PLAYERBOT_INS_BOT_ACCOUNT);
    stmt->setUInt32(0, accountId);
    PlayerbotDatabase.Execute(stmt);
    
    // Add to our cache
    {
        std::lock_guard<std::mutex> lock(_accountMutex);
        
        BotAccountInfo info;
        info.accountId = accountId;
        info.username = username;
        info.isActive = true;
        info.characterCount = 0;
        info.lastLogin = 0;
        
        _botAccounts[accountId] = info;
        _botAccountIds.insert(accountId);
        _totalAccounts++;
        _activeAccounts++;
    }
    
    TC_LOG_INFO("module.playerbot", 
        "Created bot account '{}' (ID: {})", username, accountId);
    
    return accountId;
}

bool BotAccountMgr::DeleteBotAccount(uint32 accountId)
{
    if (!IsBotAccount(accountId))
    {
        TC_LOG_ERROR("module.playerbot", 
            "Cannot delete non-bot account {}", accountId);
        return false;
    }
    
    // Check if account has characters
    uint32 charCount = GetCharacterCount(accountId);
    if (charCount > 0)
    {
        TC_LOG_ERROR("module.playerbot", 
            "Cannot delete bot account {} with {} characters", 
            accountId, charCount);
        return false;
    }
    
    // Remove from playerbot_accounts
    PreparedStatement<PlayerbotDatabaseConnection>* stmt = PlayerbotDatabase.GetPreparedStatement(PLAYERBOT_DEL_BOT_ACCOUNT);
    stmt->setUInt32(0, accountId);
    PlayerbotDatabase.Execute(stmt);
    
    // Delete the actual account
    AccountMgr::DeleteAccount(accountId);
    
    // Remove from cache
    {
        std::lock_guard<std::mutex> lock(_accountMutex);
        
        auto it = _botAccounts.find(accountId);
        if (it != _botAccounts.end())
        {
            if (it->second.isActive)
                _activeAccounts--;
            _totalAccounts--;
            _botAccounts.erase(it);
        }
        
        _botAccountIds.erase(accountId);
    }
    
    TC_LOG_INFO("module.playerbot", "Deleted bot account {}", accountId);
    
    return true;
}

bool BotAccountMgr::ValidateCharacterCount(uint32 accountId)
{
    if (!IsBotAccount(accountId))
        return true;
    
    uint32 actualCount = 0;
    
    // Get actual count from database
    PreparedStatement<PlayerbotDatabaseConnection>* stmt = PlayerbotDatabase.GetPreparedStatement(PLAYERBOT_SEL_ACCOUNT_CHARACTER_COUNT);
    stmt->setUInt32(0, accountId);
    PreparedQueryResult result = PlayerbotDatabase.Query(stmt);
    
    if (result)
    {
        Field* fields = result->Fetch();
        actualCount = fields[0].GetUInt32();
    }
    
    // Update cache
    {
        std::lock_guard<std::mutex> lock(_characterCountMutex);
        _characterCounts[accountId] = actualCount;
    }
    
    // Update playerbot_accounts table
    PreparedStatement<PlayerbotDatabaseConnection>* updateStmt = PlayerbotDatabase.GetPreparedStatement(PLAYERBOT_UPD_BOT_ACCOUNT_CHARACTER_COUNT);
    updateStmt->setUInt32(0, actualCount);
    updateStmt->setUInt32(1, accountId);
    PlayerbotDatabase.Execute(updateStmt);
    
    if (actualCount > MAX_CHARACTERS_PER_BOT_ACCOUNT)
    {
        TC_LOG_ERROR("module.playerbot", 
            "Bot account {} has {} characters, exceeds limit of {}!",
            accountId, actualCount, MAX_CHARACTERS_PER_BOT_ACCOUNT);
        
        if (sConfigMgr->GetBoolDefault("Playerbot.AutoFixCharacterLimit", false))
        {
            DisableExcessCharacters(accountId, actualCount - MAX_CHARACTERS_PER_BOT_ACCOUNT);
        }
        
        return false;
    }
    
    return true;
}

bool BotAccountMgr::ValidateAllBotAccounts()
{
    TC_LOG_INFO("module.playerbot", "Validating character counts for all bot accounts...");
    
    std::lock_guard<std::mutex> lock(_accountMutex);
    
    uint32 totalAccounts = 0;
    uint32 violationCount = 0;
    
    for (auto const& [accountId, info] : _botAccounts)
    {
        if (!ValidateCharacterCount(accountId))
        {
            violationCount++;
        }
        totalAccounts++;
    }
    
    TC_LOG_INFO("module.playerbot", 
        "Validated {} bot accounts, {} violations found",
        totalAccounts, violationCount);
    
    return violationCount == 0;
}

std::vector<BotAccountInfo> BotAccountMgr::GetAllBotAccounts() const
{
    std::lock_guard<std::mutex> lock(_accountMutex);
    
    std::vector<BotAccountInfo> accounts;
    accounts.reserve(_botAccounts.size());
    
    for (auto const& [accountId, info] : _botAccounts)
    {
        accounts.push_back(info);
    }
    
    return accounts;
}

uint32 BotAccountMgr::GetTotalBotAccounts() const
{
    std::lock_guard<std::mutex> lock(_accountMutex);
    return _totalAccounts;
}

uint32 BotAccountMgr::GetActiveBotAccounts() const
{
    std::lock_guard<std::mutex> lock(_accountMutex);
    return _activeAccounts;
}
