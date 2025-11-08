/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 */

#pragma once

#include "Define.h"
#include "DatabaseEnvFwd.h"
#include "PreparedStatement.h"
#include "QueryResult.h"
#include <functional>
#include <vector>
#include <string>
#include <chrono>
#include <atomic>

namespace Playerbot
{

/**
 * @brief Interface for Bot Database Connection Pooling
 *
 * Abstracts high-performance database operations to enable dependency injection and testing.
 * Provides async/sync query execution, prepared statement caching, and performance monitoring.
 *
 * **Responsibilities:**
 * - Async query execution with callbacks
 * - Synchronous query execution
 * - Prepared statement caching
 * - Result caching with TTL
 * - Performance metrics tracking
 *
 * **Testability:**
 * - Can be mocked for testing without real database
 * - Enables testing of data access logic in isolation
 *
 * Example:
 * @code
 * auto dbPool = Services::Container().Resolve<IBotDatabasePool>();
 * dbPool->ExecuteAsync(stmt, [](PreparedQueryResult result) {
 *     if (result) {
 *         // Process result
 *     }
 * });
 * @endcode
 */
class TC_GAME_API IBotDatabasePool
{
public:
    /**
     * @brief Database performance metrics
     */
    struct DatabaseMetrics
    {
        std::atomic<uint64> queriesExecuted{0};
        std::atomic<uint64> queriesPerSecond{0};
        std::atomic<uint64> cacheHits{0};
        std::atomic<uint64> cacheMisses{0};
        std::atomic<uint32> avgResponseTimeMs{0};
        std::atomic<uint32> activeConnections{0};
        std::atomic<uint32> maxResponseTimeMs{0};
        std::atomic<uint32> timeouts{0};
        std::atomic<uint32> errors{0};
        std::atomic<size_t> memoryUsage{0};
    };

    virtual ~IBotDatabasePool() = default;

    /**
     * @brief Initialize database pool
     * @param connectionString Database connection string
     * @param asyncThreads Number of async worker threads
     * @param syncThreads Number of sync worker threads
     * @return true if initialization successful, false otherwise
     */
    virtual bool Initialize(std::string const& connectionString,
                           uint8 asyncThreads = 4,
                           uint8 syncThreads = 2) = 0;

    /**
     * @brief Shutdown database pool
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Execute async query with callback
     * @param stmt Prepared statement to execute
     * @param callback Function called when query completes
     * @param timeoutMs Query timeout in milliseconds
     */
    virtual void ExecuteAsync(CharacterDatabasePreparedStatement* stmt,
                             std::function<void(PreparedQueryResult)> callback,
                             uint32 timeoutMs = 30000) = 0;

    /**
     * @brief Execute async query without result callback
     * @param stmt Prepared statement to execute
     * @param timeoutMs Query timeout in milliseconds
     */
    virtual void ExecuteAsyncNoResult(CharacterDatabasePreparedStatement* stmt,
                                     uint32 timeoutMs = 30000) = 0;

    /**
     * @brief Execute batch async operations
     * @param statements Vector of prepared statements
     * @param callback Function called with all results
     * @param timeoutMs Query timeout in milliseconds
     */
    virtual void ExecuteBatchAsync(std::vector<CharacterDatabasePreparedStatement*> const& statements,
                                  std::function<void(std::vector<PreparedQueryResult>)> callback,
                                  uint32 timeoutMs = 30000) = 0;

    /**
     * @brief Execute synchronous query
     * @param stmt Prepared statement to execute
     * @param timeoutMs Query timeout in milliseconds
     * @return Query result, or nullptr on failure
     */
    virtual PreparedQueryResult ExecuteSync(CharacterDatabasePreparedStatement* stmt,
                                           uint32 timeoutMs = 10000) = 0;

    /**
     * @brief Get prepared statement by ID
     * @param stmtId Statement ID
     * @return Prepared statement pointer
     */
    virtual CharacterDatabasePreparedStatement* GetPreparedStatement(uint32 stmtId) = 0;

    /**
     * @brief Cache prepared statement for reuse
     * @param stmtId Statement ID
     * @param sql SQL query string
     */
    virtual void CachePreparedStatement(uint32 stmtId, std::string const& sql) = 0;

    /**
     * @brief Cache query result with TTL
     * @param key Cache key
     * @param result Query result to cache
     * @param ttl Time to live in seconds
     */
    virtual void CacheResult(std::string const& key, PreparedQueryResult const& result,
                            std::chrono::seconds ttl = std::chrono::seconds(60)) = 0;

    /**
     * @brief Get cached result
     * @param key Cache key
     * @return Cached result, or nullptr if not found/expired
     */
    virtual PreparedQueryResult GetCachedResult(std::string const& key) = 0;

    /**
     * @brief Get performance metrics
     * @return Database metrics structure
     */
    virtual DatabaseMetrics const& GetMetrics() const = 0;

    /**
     * @brief Get cache hit rate
     * @return Cache hit rate (0.0 - 1.0)
     */
    virtual double GetCacheHitRate() const = 0;

    /**
     * @brief Get average response time
     * @return Average response time in milliseconds
     */
    virtual uint32 GetAverageResponseTime() const = 0;

    /**
     * @brief Check if database pool is healthy
     * @return true if healthy, false if experiencing errors
     */
    virtual bool IsHealthy() const = 0;

    /**
     * @brief Set query timeout
     * @param timeoutMs Timeout in milliseconds
     */
    virtual void SetQueryTimeout(uint32 timeoutMs) = 0;

    /**
     * @brief Set cache size
     * @param maxSize Maximum cache size
     */
    virtual void SetCacheSize(size_t maxSize) = 0;

    /**
     * @brief Set connection recycle interval
     * @param interval Recycle interval
     */
    virtual void SetConnectionRecycleInterval(std::chrono::seconds interval) = 0;
};

} // namespace Playerbot
