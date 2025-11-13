/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#pragma once

#include "Define.h"
#include "DatabaseEnvFwd.h"
#include "CharacterDatabase.h"
#include <string>
#include <functional>
#include <thread>

namespace Playerbot
{

/**
 * @brief Interface for playerbot character database operations
 *
 * Provides safe sync/async database access with automatic routing,
 * transaction support, and error handling.
 */
class IPlayerbotCharacterDBInterface
{
public:
    virtual ~IPlayerbotCharacterDBInterface() = default;

    // Lifecycle
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Update(uint32 diff) = 0;

    // Statement operations
    virtual CharacterDatabasePreparedStatement* GetPreparedStatement(CharacterDatabaseStatements statementId) = 0;
    virtual PreparedQueryResult ExecuteSync(CharacterDatabasePreparedStatement* stmt) = 0;

    // Transaction operations
    virtual CharacterDatabaseTransaction BeginTransaction() = 0;
    virtual void CommitTransaction(CharacterDatabaseTransaction trans, bool async = true) = 0;

    // Direct SQL (for migrations only)
    virtual bool ExecuteDirectSQL(std::string const& sql) = 0;

    // Context checking
    virtual bool IsAsyncContext() const = 0;
    virtual bool IsSyncOnlyStatement(uint32 statementId) const = 0;
    virtual std::thread::id GetMainThreadId() const = 0;

    // Configuration and metrics
    virtual void ResetMetrics() = 0;

    // Queue processing
    virtual void ProcessSyncQueue() = 0;

    // Initialization helpers
    virtual void InitializeStatementClassification() = 0;
};

} // namespace Playerbot
