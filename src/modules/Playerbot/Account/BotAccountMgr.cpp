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

#include "BotAccountMgr.h"
#include "AccountMgr.h"
#include "BattlenetAccountMgr.h"
#include "PlayerbotConfig.h"
#include "Log.h"
#include "Util.h"
#include <sstream>
#include <iomanip>
#include <random>
#include <thread>

namespace Playerbot {

bool BotAccountMgr::Initialize()
{
    TC_LOG_INFO("module.playerbot.account", "Initializing BotAccountMgr...");

    // Load configuration values from playerbots.conf
    LoadConfigurationValues();

    // Load existing bot accounts from database
    LoadAccountMetadata();

    // Check if automatic account creation is enabled
    if (_autoCreateAccounts.load())
    {
        uint32 required = GetRequiredAccountCount();
        uint32 current = _totalAccounts.load();

        TC_LOG_INFO("module.playerbot.account",
            "Auto account creation enabled: {} required, {} exist",
            required, current);

        // Start account pool refill if needed
        if (current < required)
        {
            TC_LOG_INFO("module.playerbot.account",
                "Creating {} additional bot accounts...", required - current);
            RefillAccountPool();
        }
    }
    else
    {
        TC_LOG_INFO("module.playerbot.account",
            "Auto account creation disabled in configuration");
    }

    TC_LOG_INFO("module.playerbot.account",
        "✅ BotAccountMgr initialized: {} accounts, {} in pool, auto-create: {}",
        _totalAccounts.load(), GetPoolSize(), _autoCreateAccounts.load());

    return true;
}

void BotAccountMgr::Shutdown()
{
    TC_LOG_INFO("module.playerbot.account", "Shutting down BotAccountMgr...");

    // Save all account metadata
    for (auto const& [id, info] : _accounts)
    {
        StoreAccountMetadata(info);
    }

    TC_LOG_INFO("module.playerbot.account",
        "✅ BotAccountMgr shutdown: {} accounts saved", _accounts.size());
}

void BotAccountMgr::LoadConfigurationValues()
{
    TC_LOG_DEBUG("module.playerbot.account", "Loading configuration values...");

    // Load configuration from playerbots.conf
    _maxBotsTotal.store(sPlayerbotConfig->GetUInt("Playerbot.MaxBotsTotal", 1000));
    _autoCreateAccounts.store(sPlayerbotConfig->GetBool("Playerbot.AutoCreateAccounts", false));
    _accountsToCreate.store(sPlayerbotConfig->GetUInt("Playerbot.AccountsToCreate", 0));

    // Calculate required accounts based on configuration logic
    uint32 calculatedAccounts = _maxBotsTotal.load() / 10;
    uint32 configuredAccounts = _accountsToCreate.load();

    // Use configured value if it's greater than calculated, otherwise use calculated
    uint32 requiredAccounts = (configuredAccounts > calculatedAccounts) ? configuredAccounts : calculatedAccounts;
    _requiredAccounts.store(requiredAccounts);

    // Set target pool size (keep 25% in pool for instant availability)
    _targetPoolSize.store(std::max(10U, requiredAccounts / 4));

    TC_LOG_INFO("module.playerbot.account",
        "Configuration loaded: MaxBotsTotal={}, AutoCreate={}, AccountsToCreate={}, Required={}, PoolTarget={}",
        _maxBotsTotal.load(), _autoCreateAccounts.load(), _accountsToCreate.load(),
        _requiredAccounts.load(), _targetPoolSize.load());
}

void BotAccountMgr::UpdateConfiguration()
{
    LoadConfigurationValues();

    // If auto-create is enabled and we need more accounts, start creating them
    if (_autoCreateAccounts.load())
    {
        uint32 required = GetRequiredAccountCount();
        uint32 current = _totalAccounts.load();

        if (current < required)
        {
            TC_LOG_INFO("module.playerbot.account",
                "Configuration changed: creating {} additional accounts", required - current);
            RefillAccountPool();
        }
    }
}

uint32 BotAccountMgr::GetRequiredAccountCount() const
{
    return _requiredAccounts.load();
}

uint32 BotAccountMgr::CreateBotAccount(std::string const& requestedEmail)
{
    // Check if auto-creation is disabled
    if (!_autoCreateAccounts.load())
    {
        TC_LOG_WARN("module.playerbot.account",
            "Account creation requested but Playerbot.AutoCreateAccounts is disabled");
        return 0;
    }

    // Check account limit
    uint32 required = GetRequiredAccountCount();
    if (_totalAccounts.load() >= required)
    {
        TC_LOG_ERROR("module.playerbot.account",
            "Cannot create account: limit {} reached", required);
        return 0;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    // Generate email and password
    std::string email = requestedEmail.empty() ? GenerateUniqueEmail() : requestedEmail;
    std::string password = GenerateSecurePassword();

    // Create BattleNet account using Trinity's system
    std::string gameAccountName;
    AccountOpResult result = Battlenet::AccountMgr::CreateBattlenetAccount(email, password, true, &gameAccountName);

    if (result != AccountOpResult::AOR_OK)
    {
        TC_LOG_ERROR("module.playerbot.account",
            "Failed to create BattleNet account for email: {}, result: {}", email, static_cast<uint32>(result));
        return 0;
    }

    // Get the created game account ID
    uint32 legacyAccountId = AccountMgr::GetId(gameAccountName);
    if (legacyAccountId == 0)
    {
        TC_LOG_ERROR("module.playerbot.account",
            "Failed to retrieve legacy account ID for game account: {}", gameAccountName);
        return 0;
    }

    // For now, use legacyAccountId as the primary identifier since we can't easily get BNet account ID
    uint32 bnetAccountId = legacyAccountId;

    // Create account info
    BotAccountInfo info;
    info.bnetAccountId = bnetAccountId;
    info.legacyAccountId = legacyAccountId;
    info.email = email;
    info.passwordHash = password; // Store hash in production
    info.createdAt = std::chrono::system_clock::now();
    info.characterCount = 0;
    info.isActive = false;
    info.isInPool = false;

    // Store in memory
    {
        std::unique_lock lock(_accountsMutex);
        _accounts[bnetAccountId] = info;
    }

    // Store in database asynchronously
    StoreAccountMetadata(info);

    // Update statistics
    _totalAccounts.fetch_add(1);

    auto endTime = std::chrono::high_resolution_clock::now();
    uint32 creationTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - startTime).count();

    TC_LOG_DEBUG("module.playerbot.account",
        "Created bot account: BNet {} (Legacy {}), Email: {}, Time: {}μs",
        bnetAccountId, legacyAccountId, email, creationTimeUs);

    return bnetAccountId;
}

void BotAccountMgr::CreateBotAccountsBatch(uint32 count,
    std::function<void(std::vector<uint32>)> callback)
{
    TC_LOG_INFO("module.playerbot.account", "Creating batch of {} bot accounts...", count);

    // Execute in a separate thread to avoid blocking
    std::thread([this, count, callback]()
    {
        std::vector<uint32> createdAccounts;
        createdAccounts.reserve(count);

        for (uint32 i = 0; i < count; ++i)
        {
            if (uint32 accountId = CreateBotAccount())
            {
                createdAccounts.push_back(accountId);
            }
            else
            {
                // If creation fails, we might have hit the limit
                break;
            }

            // Small delay to avoid overwhelming the system
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        TC_LOG_INFO("module.playerbot.account",
            "Batch creation complete: {}/{} accounts created",
            createdAccounts.size(), count);

        if (callback)
        {
            callback(createdAccounts);
        }
    }).detach();
}

void BotAccountMgr::RefillAccountPool()
{
    // Check if refill already in progress
    uint32 expected = 0;
    if (!_poolRefillInProgress.compare_exchange_strong(expected, 1))
    {
        return; // Already refilling
    }

    uint32 targetPoolSize = _targetPoolSize.load();

    TC_LOG_INFO("module.playerbot.account",
        "Refilling account pool to {} accounts...", targetPoolSize);

    // Calculate how many accounts to create
    uint32 currentSize = GetPoolSize();
    uint32 requiredTotal = GetRequiredAccountCount();
    uint32 existingTotal = _totalAccounts.load();

    // Accounts needed for total requirement
    uint32 totalToCreate = (existingTotal < requiredTotal) ? (requiredTotal - existingTotal) : 0;

    // Additional accounts needed for pool
    uint32 poolToCreate = (currentSize < targetPoolSize) ? (targetPoolSize - currentSize) : 0;

    uint32 toCreate = std::max(totalToCreate, poolToCreate);

    if (toCreate > 0 && _autoCreateAccounts.load())
    {
        CreateBotAccountsBatch(toCreate,
            [this, targetPoolSize](std::vector<uint32> const& accounts)
            {
                // Add to pool
                {
                    std::lock_guard<std::mutex> lock(_poolMutex);
                    for (uint32 accountId : accounts)
                    {
                        _accountPool.push(accountId);

                        // Mark as in pool
                        auto it = _accounts.find(accountId);
                        if (it != _accounts.end())
                        {
                            it->second.isInPool = true;
                        }
                    }
                }

                TC_LOG_INFO("module.playerbot.account",
                    "✅ Account pool refilled: {} accounts now available",
                    GetPoolSize());

                _poolRefillInProgress.store(0);
            });
    }
    else
    {
        if (!_autoCreateAccounts.load())
        {
            TC_LOG_INFO("module.playerbot.account",
                "Account pool refill skipped: auto-creation disabled");
        }
        _poolRefillInProgress.store(0);
    }
}

uint32 BotAccountMgr::AcquireAccount()
{
    // Try to get from pool first
    {
        std::lock_guard<std::mutex> lock(_poolMutex);
        if (!_accountPool.empty())
        {
            uint32 accountId = _accountPool.front();
            _accountPool.pop();

            // Mark as active
            auto it = _accounts.find(accountId);
            if (it != _accounts.end())
            {
                it->second.isActive = true;
                it->second.isInPool = false;
            }

            _activeAccounts.fetch_add(1);

            TC_LOG_DEBUG("module.playerbot.account",
                "Acquired account {} from pool", accountId);

            // Trigger pool refill if getting low
            if (_accountPool.size() < _targetPoolSize.load() / 2)
            {
                RefillAccountPool();
            }

            return accountId;
        }
    }

    // Pool empty, create new account if auto-creation is enabled
    if (_autoCreateAccounts.load())
    {
        TC_LOG_DEBUG("module.playerbot.account",
            "Account pool empty, creating new account...");

        uint32 newAccountId = CreateBotAccount();

        if (newAccountId > 0)
        {
            // Mark as active
            auto it = _accounts.find(newAccountId);
            if (it != _accounts.end())
            {
                it->second.isActive = true;
            }
            _activeAccounts.fetch_add(1);
        }

        return newAccountId;
    }
    else
    {
        TC_LOG_WARN("module.playerbot.account",
            "Cannot acquire account: pool empty and auto-creation disabled");
        return 0;
    }
}

void BotAccountMgr::ReleaseAccount(uint32 bnetAccountId)
{
    std::lock_guard<std::mutex> lock(_poolMutex);

    // Check if account exists
    auto it = _accounts.find(bnetAccountId);
    if (it == _accounts.end())
    {
        TC_LOG_WARN("module.playerbot.account",
            "Cannot release unknown account {}", bnetAccountId);
        return;
    }

    uint32 targetPoolSize = _targetPoolSize.load();

    // Return to pool if under limit
    if (_accountPool.size() < targetPoolSize * 2) // Allow some overflow
    {
        _accountPool.push(bnetAccountId);
        it->second.isActive = false;
        it->second.isInPool = true;
        _activeAccounts.fetch_sub(1);

        TC_LOG_DEBUG("module.playerbot.account",
            "Released account {} back to pool", bnetAccountId);
    }
    else
    {
        // Pool full, just mark as inactive
        it->second.isActive = false;
        it->second.isInPool = false;
        _activeAccounts.fetch_sub(1);

        TC_LOG_DEBUG("module.playerbot.account",
            "Released account {} (pool full)", bnetAccountId);
    }
}

uint32 BotAccountMgr::GetPoolSize() const
{
    std::lock_guard<std::mutex> lock(_poolMutex);
    return _accountPool.size();
}

bool BotAccountMgr::CanCreateCharacter(uint32 bnetAccountId) const
{
    std::shared_lock lock(_accountsMutex);

    auto it = _accounts.find(bnetAccountId);
    if (it == _accounts.end())
    {
        return false;
    }

    return it->second.characterCount < _maxCharactersPerAccount.load();
}

void BotAccountMgr::UpdateCharacterCount(uint32 bnetAccountId, int8 delta)
{
    std::unique_lock lock(_accountsMutex);

    auto it = _accounts.find(bnetAccountId);
    if (it != _accounts.end())
    {
        int16 newCount = it->second.characterCount + delta;
        it->second.characterCount = std::max(0, std::min(static_cast<int>(
            _maxCharactersPerAccount.load()), static_cast<int>(newCount)));

        TC_LOG_DEBUG("module.playerbot.account",
            "Updated character count for account {}: {} characters",
            bnetAccountId, it->second.characterCount);

        // Update metadata in database
        StoreAccountMetadata(it->second);
    }
}

BotAccountMgr::BotAccountInfo const* BotAccountMgr::GetAccountInfo(uint32 bnetAccountId) const
{
    std::shared_lock lock(_accountsMutex);

    auto it = _accounts.find(bnetAccountId);
    return (it != _accounts.end()) ? &it->second : nullptr;
}

void BotAccountMgr::DeleteBotAccount(uint32 bnetAccountId,
    std::function<void(bool success)> callback)
{
    TC_LOG_INFO("module.playerbot.account",
        "Deleting bot account {}...", bnetAccountId);

    // For our simplified implementation, the bnetAccountId is actually the legacy account ID
    uint32 legacyAccountId = bnetAccountId;

    // Delete the account (this will delete characters too)
    AccountOpResult result = AccountMgr::DeleteAccount(legacyAccountId);

    if (result == AccountOpResult::AOR_OK)
    {
        // Remove from memory
        {
            std::unique_lock lock(_accountsMutex);
            _accounts.erase(bnetAccountId);
        }

        _totalAccounts.fetch_sub(1);

        TC_LOG_INFO("module.playerbot.account",
            "✅ Deleted bot account {}", bnetAccountId);

        if (callback) callback(true);
    }
    else
    {
        TC_LOG_ERROR("module.playerbot.account",
            "Failed to delete account {}: {}", bnetAccountId, static_cast<uint32>(result));
        if (callback) callback(false);
    }
}

void BotAccountMgr::DeleteAllBotAccounts(std::function<void(uint32 deleted)> callback)
{
    TC_LOG_WARN("module.playerbot.account", "Deleting ALL bot accounts...");

    std::vector<uint32> accountIds;
    {
        std::shared_lock lock(_accountsMutex);
        accountIds.reserve(_accounts.size());
        for (auto const& [id, info] : _accounts)
        {
            accountIds.push_back(id);
        }
    }

    if (accountIds.empty())
    {
        TC_LOG_INFO("module.playerbot.account", "No bot accounts to delete");
        if (callback) callback(0);
        return;
    }

    // Delete accounts in a separate thread
    std::thread([this, accountIds = std::move(accountIds), callback]()
    {
        uint32 deleted = 0;
        for (uint32 accountId : accountIds)
        {
            bool success = false;
            DeleteBotAccount(accountId, [&success](bool result) { success = result; });

            if (success)
            {
                ++deleted;
            }

            // Small delay between deletions
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        TC_LOG_WARN("module.playerbot.account",
            "✅ Deleted {}/{} bot accounts", deleted, accountIds.size());

        if (callback) callback(deleted);
    }).detach();
}

std::string BotAccountMgr::GenerateUniqueEmail()
{
    uint32 counter = _emailCounter.fetch_add(1);

    std::ostringstream email;
    email << "bot" << std::setfill('0') << std::setw(6) << counter
          << "@" << _emailDomain;

    return email.str();
}

std::string BotAccountMgr::GenerateSecurePassword()
{
    // Generate random secure password
    static const char charset[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"
        "!@#$%^&*";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);

    std::string password;
    password.reserve(16);

    for (int i = 0; i < 16; ++i)
    {
        password += charset[dis(gen)];
    }

    return password;
}

void BotAccountMgr::StoreAccountMetadata(BotAccountInfo const& info)
{
    // TODO: Implement database storage when BotDatabasePool is available
    // For now, log the metadata
    TC_LOG_DEBUG("module.playerbot.account",
        "Storing metadata for account {}: email={}, characters={}",
        info.bnetAccountId, info.email, info.characterCount);
}

void BotAccountMgr::LoadAccountMetadata()
{
    // TODO: Implement database loading when BotDatabasePool is available
    TC_LOG_INFO("module.playerbot.account", "Loading bot account metadata...");

    // For now, just initialize empty
    _totalAccounts.store(0);

    TC_LOG_INFO("module.playerbot.account", "✅ Loaded 0 bot account metadata entries");
}

} // namespace Playerbot