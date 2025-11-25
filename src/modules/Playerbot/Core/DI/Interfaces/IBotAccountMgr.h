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

#pragma once

#include "Define.h"
#include <string>
#include <vector>
#include <functional>
#include <chrono>

namespace Playerbot
{

/**
 * @brief Interface for Bot Account Management
 *
 * Abstracts bot account creation, pooling, and management to enable
 * dependency injection and testing.
 *
 * **Responsibilities:**
 * - Create and manage bot BattleNet accounts
 * - Maintain account pool for instant availability
 * - Track account usage and character counts
 * - Handle account deletion
 * - Enforce character limits per account
 *
 * **Testability:**
 * - Can be mocked for testing without real database/accounts
 * - Enables testing account management logic in isolation
 *
 * Example:
 * @code
 * auto accountMgr = Services::Container().Resolve<IBotAccountMgr>();
 * uint32 accountId = accountMgr->AcquireAccount();
 * if (accountId > 0)
 {
 *     // Use account for bot
 * }
 * @endcode
 */
class TC_GAME_API IBotAccountMgr
{
public:
    /**
     * @brief Bot account information
     */
    struct BotAccountInfo
    {
        uint32 bnetAccountId;
        uint32 legacyAccountId;
        ::std::string email;
        ::std::string passwordHash;
        ::std::chrono::system_clock::time_point createdAt;
        uint8 characterCount;
        bool isActive;
        bool isInPool;
    };

    virtual ~IBotAccountMgr() = default;

    /**
     * @brief Initialize account manager
     * @return true if successful, false otherwise
     */
    virtual bool Initialize() = 0;

    /**
     * @brief Shutdown account manager
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Update account manager (call from main thread)
     * @param diff Time since last update in milliseconds
     */
    virtual void Update(uint32 diff) = 0;

    /**
     * @brief Process pending callbacks (thread-safe)
     */
    virtual void ProcessPendingCallbacks() = 0;

    /**
     * @brief Create a new bot account
     * @param requestedEmail Optional specific email
     * @return BattleNet account ID, 0 on failure
     */
    virtual uint32 CreateBotAccount(::std::string const& requestedEmail = "") = 0;

    /**
     * @brief Batch create multiple accounts
     * @param count Number of accounts to create
     * @param callback Async callback with created account IDs
     */
    virtual void CreateBotAccountsBatch(uint32 count,
        ::std::function<void(::std::vector<uint32>)> callback) = 0;

    /**
     * @brief Pre-create accounts for instant availability
     */
    virtual void RefillAccountPool() = 0;

    /**
     * @brief Get account from pool or create new
     * @return Account ID from pool or newly created
     */
    virtual uint32 AcquireAccount() = 0;

    /**
     * @brief Return account to pool when bot logs out
     * @param bnetAccountId Account ID to release
     */
    virtual void ReleaseAccount(uint32 bnetAccountId) = 0;

    /**
     * @brief Get account information
     * @param bnetAccountId BattleNet account ID
     * @return Account info pointer, nullptr if not found
     */
    virtual BotAccountInfo const* GetAccountInfo(uint32 bnetAccountId) const = 0;

    /**
     * @brief Get total account count
     * @return Total number of bot accounts
     */
    virtual uint32 GetTotalAccountCount() const = 0;

    /**
     * @brief Get total bot accounts (alias for compatibility)
     * @return Total number of bot accounts
     */
    virtual uint32 GetTotalBotAccounts() const = 0;

    /**
     * @brief Get active account count
     * @return Number of active bot accounts
     */
    virtual uint32 GetActiveAccountCount() const = 0;

    /**
     * @brief Get pool size
     * @return Number of accounts in pool
     */
    virtual uint32 GetPoolSize() const = 0;

    /**
     * @brief Delete bot account and all characters
     * @param bnetAccountId BattleNet account to delete
     * @param callback Optional async completion callback
     */
    virtual void DeleteBotAccount(uint32 bnetAccountId,
        ::std::function<void(bool success)> callback = nullptr) = 0;

    /**
     * @brief Delete all bot accounts (cleanup)
     * @param callback Optional callback with delete count
     */
    virtual void DeleteAllBotAccounts(::std::function<void(uint32 deleted)> callback = nullptr) = 0;

    /**
     * @brief Check if account can create more characters
     * @param bnetAccountId Account to check
     * @return true if under 10 character limit
     */
    virtual bool CanCreateCharacter(uint32 bnetAccountId) const = 0;

    /**
     * @brief Update character count for account
     * @param bnetAccountId Account ID
     * @param delta Change in character count (+1 or -1)
     */
    virtual void UpdateCharacterCount(uint32 bnetAccountId, int8 delta) = 0;

    /**
     * @brief Update configuration from playerbots.conf
     */
    virtual void UpdateConfiguration() = 0;

    /**
     * @brief Get calculated number of accounts needed
     * @return Required account count based on config
     */
    virtual uint32 GetRequiredAccountCount() const = 0;

    /**
     * @brief Check if automatic account creation is enabled
     * @return true if enabled, false otherwise
     */
    virtual bool IsAutoCreateEnabled() const = 0;
};

} // namespace Playerbot
