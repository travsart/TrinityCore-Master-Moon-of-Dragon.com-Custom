/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 */

#pragma once

#include "Define.h"
#include <cstdint>
#include <memory>

namespace Playerbot
{

class DatabaseConnection;
class PreparedStatement;
class QueryResult;

/**
 * @brief Interface for Database Connection Pooling
 *
 * Abstracts database operations to enable dependency injection and testing.
 * Manages pool of database connections for bot-related queries.
 *
 * **Testability:**
 * - Can be mocked for testing without real database
 * - Enables testing of data access logic in isolation
 */
class TC_GAME_API IDatabasePool
{
public:
    virtual ~IDatabasePool() = default;

    /**
     * @brief Initialize database pool
     *
     * @return True if initialization successful, false otherwise
     */
    virtual bool Initialize() = 0;

    /**
     * @brief Shutdown database pool
     *
     * Closes all connections and cleanup resources.
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Execute synchronous query
     *
     * @param sql SQL query string
     * @return Query result, or nullptr on failure
     */
    virtual std::shared_ptr<QueryResult> Query(char const* sql) = 0;

    /**
     * @brief Execute prepared statement
     *
     * @param stmt Prepared statement to execute
     * @return Query result, or nullptr on failure
     */
    virtual std::shared_ptr<QueryResult> Query(std::shared_ptr<PreparedStatement> stmt) = 0;

    /**
     * @brief Execute async query with callback
     *
     * @param sql SQL query string
     * @param callback Function called when query completes
     */
    virtual void AsyncQuery(char const* sql, std::function<void(std::shared_ptr<QueryResult>)> callback) = 0;

    /**
     * @brief Get pool size (number of connections)
     *
     * @return Connection pool size
     */
    virtual uint32 GetPoolSize() const = 0;

    /**
     * @brief Get number of active connections
     *
     * @return Active connection count
     */
    virtual uint32 GetActiveConnections() const = 0;
};

} // namespace Playerbot
