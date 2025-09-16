/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "BotDatabasePool.h"
#include "Log.h"
#include "DatabaseEnv.h"
#include "MySQLConnection.h"
#include <algorithm>
#include <sstream>

namespace Playerbot {

bool BotDatabasePool::Initialize(std::string const& connectionString, uint8 asyncThreads, uint8 syncThreads)
{
    if (_initialized.load()) {
        TC_LOG_WARN("module.playerbot.database", "BotDatabasePool already initialized");
        return true;
    }

    TC_LOG_INFO("module.playerbot.database", "Initializing BotDatabasePool...");

    _connectionString = connectionString;
    _asyncThreads = asyncThreads;
    _syncThreads = syncThreads;

    // Initialize Boost.Asio context
    _ioContext = std::make_unique<boost::asio::io_context>();
    _workGuard = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
        _ioContext->get_executor());

    // Initialize connection pool
    if (!InitializeConnections()) {
        TC_LOG_ERROR("module.playerbot.database", "Failed to initialize database connections");
        return false;
    }

    // Start worker threads
    StartWorkerThreads();

    // Initialize timing
    _startTime = std::chrono::steady_clock::now();
    _lastMetricsUpdate = _startTime;
    _lastConnectionRecycle = _startTime;

    _initialized.store(true);

    TC_LOG_INFO("module.playerbot.database",
        "✅ BotDatabasePool initialized: {} async + {} sync threads, {} connections",
        _asyncThreads, _syncThreads, _connections.size());

    return true;
}

void BotDatabasePool::Shutdown()
{
    if (!_initialized.load() || _shutdown.load()) {
        return;
    }

    TC_LOG_INFO("module.playerbot.database", "Shutting down BotDatabasePool...");

    _shutdown.store(true);

    // Stop accepting new queries
    _workGuard.reset();

    // Stop worker threads
    StopWorkerThreads();

    // Shutdown connections
    ShutdownConnections();

    // Log final metrics
    TC_LOG_INFO("module.playerbot.database",
        "Final metrics: {} queries executed, {:.1f}% cache hit rate, {}ms avg response time",
        _metrics.queriesExecuted.load(),
        GetCacheHitRate(),
        _metrics.avgResponseTimeMs.load());

    _initialized.store(false);

    TC_LOG_INFO("module.playerbot.database", "✅ BotDatabasePool shutdown complete");
}

void BotDatabasePool::ExecuteAsync(PreparedStatement* stmt,
                                  std::function<void(PreparedQueryResult)> callback,
                                  uint32 timeoutMs)
{
    if (!stmt) {
        TC_LOG_ERROR("module.playerbot.database", "Cannot execute null statement");
        if (callback) callback(nullptr);
        return;
    }

    if (!_initialized.load() || _shutdown.load()) {
        TC_LOG_ERROR("module.playerbot.database", "BotDatabasePool not initialized or shutting down");
        if (callback) callback(nullptr);
        return;
    }

    // Check cache first
    std::string cacheKey = GenerateCacheKey(stmt);
    PreparedQueryResult cachedResult = GetCachedResult(cacheKey);
    if (cachedResult) {
        _metrics.cacheHits.fetch_add(1, std::memory_order_relaxed);
        if (callback) callback(cachedResult);
        return;
    }

    _metrics.cacheMisses.fetch_add(1, std::memory_order_relaxed);

    // Create query request
    auto request = std::make_unique<QueryRequest>();
    request->statement = stmt;
    request->callback = std::move(callback);
    request->submitTime = std::chrono::steady_clock::now();
    request->timeoutMs = timeoutMs;
    request->requestId = _nextRequestId.fetch_add(1, std::memory_order_relaxed);

    // Submit to queue
    if (!_queryQueue.push(request.release())) {
        TC_LOG_WARN("module.playerbot.database", "Query queue full, dropping request");
        _metrics.errors.fetch_add(1, std::memory_order_relaxed);
        if (request->callback) request->callback(nullptr);
    }
}

void BotDatabasePool::ExecuteAsyncNoResult(PreparedStatement* stmt, uint32 timeoutMs)
{
    ExecuteAsync(stmt, nullptr, timeoutMs);
}

void BotDatabasePool::ExecuteBatchAsync(std::vector<PreparedStatement*> const& statements,
                                       std::function<void(std::vector<PreparedQueryResult>)> callback,
                                       uint32 timeoutMs)
{
    if (statements.empty()) {
        if (callback) callback({});
        return;
    }

    // For batch operations, we'll execute them sequentially in async context
    // A more advanced implementation could use parallel execution
    auto results = std::make_shared<std::vector<PreparedQueryResult>>();
    results->reserve(statements.size());

    auto counter = std::make_shared<std::atomic<size_t>>(0);

    for (auto* stmt : statements) {
        ExecuteAsync(stmt, [results, counter, callback, total = statements.size()](PreparedQueryResult result) {
            results->push_back(result);

            if (counter->fetch_add(1, std::memory_order_relaxed) + 1 == total) {
                if (callback) callback(*results);
            }
        }, timeoutMs);
    }
}

PreparedQueryResult BotDatabasePool::ExecuteSync(PreparedStatement* stmt, uint32 timeoutMs)
{
    if (!stmt) {
        TC_LOG_ERROR("module.playerbot.database", "Cannot execute null statement");
        return nullptr;
    }

    if (!_initialized.load() || _shutdown.load()) {
        TC_LOG_ERROR("module.playerbot.database", "BotDatabasePool not initialized or shutting down");
        return nullptr;
    }

    // Check cache first
    std::string cacheKey = GenerateCacheKey(stmt);
    PreparedQueryResult cachedResult = GetCachedResult(cacheKey);
    if (cachedResult) {
        _metrics.cacheHits.fetch_add(1, std::memory_order_relaxed);
        return cachedResult;
    }

    _metrics.cacheMisses.fetch_add(1, std::memory_order_relaxed);

    auto startTime = std::chrono::steady_clock::now();

    // Acquire connection
    size_t connectionIndex = AcquireConnection();
    if (connectionIndex == SIZE_MAX) {
        TC_LOG_ERROR("module.playerbot.database", "No available connections for sync query");
        _metrics.errors.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }

    PreparedQueryResult result = nullptr;

    try {
        auto& connectionInfo = _connections[connectionIndex];
        if (connectionInfo && connectionInfo->connection) {
            // Execute query synchronously
            result = connectionInfo->connection->Query(stmt);
            connectionInfo->queryCount++;

            // Cache the result
            if (result) {
                CacheResult(cacheKey, result);
            }
        }
    }
    catch (std::exception const& e) {
        TC_LOG_ERROR("module.playerbot.database",
            "Exception during sync query execution: {}", e.what());
        _metrics.errors.fetch_add(1, std::memory_order_relaxed);
    }

    // Release connection
    ReleaseConnection(connectionIndex);

    // Record metrics
    RecordQueryExecution(startTime);

    return result;
}

PreparedStatement* BotDatabasePool::GetPreparedStatement(uint32 stmtId)
{
    auto it = _preparedStatements.find(stmtId);
    if (it != _preparedStatements.end()) {
        // Create new PreparedStatement from cached SQL
        return new PreparedStatement(stmtId, it->second);
    }

    TC_LOG_WARN("module.playerbot.database",
        "Prepared statement {} not found in cache", stmtId);
    return nullptr;
}

void BotDatabasePool::CachePreparedStatement(uint32 stmtId, std::string const& sql)
{
    _preparedStatements[stmtId] = sql;
    TC_LOG_DEBUG("module.playerbot.database",
        "Cached prepared statement {}: {}", stmtId, sql);
}

void BotDatabasePool::CacheResult(std::string const& key, PreparedQueryResult const& result,
                                 std::chrono::seconds ttl)
{
    if (!result || key.empty()) return;

    // Check cache size limit
    if (_resultCache.size() >= _maxCacheSize.load()) {
        EvictLeastRecentlyUsed();
    }

    CacheEntry entry;
    entry.result = result;
    entry.expiry = std::chrono::steady_clock::now() + ttl;
    entry.lastAccess = std::chrono::steady_clock::now();
    entry.accessCount = 1;

    _resultCache[key] = entry;
}

PreparedQueryResult BotDatabasePool::GetCachedResult(std::string const& key)
{
    auto it = _resultCache.find(key);
    if (it == _resultCache.end()) {
        return nullptr;
    }

    auto& entry = it->second;

    // Check expiry
    if (std::chrono::steady_clock::now() > entry.expiry) {
        _resultCache.erase(it);
        return nullptr;
    }

    // Update access info
    entry.lastAccess = std::chrono::steady_clock::now();
    entry.accessCount++;

    return entry.result;
}

double BotDatabasePool::GetCacheHitRate() const
{
    uint64 hits = _metrics.cacheHits.load();
    uint64 misses = _metrics.cacheMisses.load();
    uint64 total = hits + misses;

    return total > 0 ? (static_cast<double>(hits) / total) * 100.0 : 0.0;
}

bool BotDatabasePool::IsHealthy() const
{
    // Consider healthy if:
    // - Average response time < 10ms
    // - Error rate < 1%
    // - At least 50% of connections are available

    uint32 avgResponseTime = _metrics.avgResponseTimeMs.load();
    uint64 totalQueries = _metrics.queriesExecuted.load();
    uint32 errors = _metrics.errors.load();
    uint32 activeConnections = _metrics.activeConnections.load();

    bool responseTimeOk = avgResponseTime < 10;
    bool errorRateOk = totalQueries == 0 || (static_cast<double>(errors) / totalQueries) < 0.01;
    bool connectionsOk = activeConnections < (_connections.size() / 2);

    return responseTimeOk && errorRateOk && connectionsOk;
}

// === PRIVATE IMPLEMENTATION ===

bool BotDatabasePool::InitializeConnections()
{
    uint32 totalConnections = _asyncThreads + _syncThreads;
    _connections.reserve(totalConnections);

    for (uint32 i = 0; i < totalConnections; ++i) {
        auto connectionInfo = std::make_unique<ConnectionInfo>();

        try {
            // Create MySQL connection
            // Note: This is a simplified example - actual implementation would
            // need proper MySQL connection setup with the connection string
            connectionInfo->connection = std::make_unique<MySQLConnection>(/* connection params */);
            connectionInfo->lastUsed = std::chrono::steady_clock::now();

            _connections.push_back(std::move(connectionInfo));
            _availableConnections.push(i);

            TC_LOG_DEBUG("module.playerbot.database",
                "Initialized database connection {}", i);
        }
        catch (std::exception const& e) {
            TC_LOG_ERROR("module.playerbot.database",
                "Failed to initialize connection {}: {}", i, e.what());
            return false;
        }
    }

    TC_LOG_INFO("module.playerbot.database",
        "Initialized {} database connections", _connections.size());

    return true;
}

void BotDatabasePool::ShutdownConnections()
{
    std::unique_lock lock(_connectionMutex);

    for (auto& connectionInfo : _connections) {
        if (connectionInfo && connectionInfo->connection) {
            // Close connection gracefully
            connectionInfo->connection.reset();
        }
    }

    _connections.clear();

    // Clear available connections queue
    size_t connectionIndex;
    while (_availableConnections.pop(connectionIndex)) {
        // Just drain the queue
    }
}

size_t BotDatabasePool::AcquireConnection()
{
    size_t connectionIndex;
    if (_availableConnections.pop(connectionIndex)) {
        if (connectionIndex < _connections.size()) {
            auto& connectionInfo = _connections[connectionIndex];
            if (connectionInfo) {
                connectionInfo->inUse.store(true);
                connectionInfo->lastUsed = std::chrono::steady_clock::now();
                _metrics.activeConnections.fetch_add(1, std::memory_order_relaxed);
                return connectionIndex;
            }
        }
    }

    return SIZE_MAX; // No available connections
}

void BotDatabasePool::ReleaseConnection(size_t connectionIndex)
{
    if (connectionIndex >= _connections.size()) return;

    auto& connectionInfo = _connections[connectionIndex];
    if (connectionInfo) {
        connectionInfo->inUse.store(false);
        _availableConnections.push(connectionIndex);
        _metrics.activeConnections.fetch_sub(1, std::memory_order_relaxed);
    }
}

void BotDatabasePool::StartWorkerThreads()
{
    _workers.reserve(_asyncThreads);

    for (uint8 i = 0; i < _asyncThreads; ++i) {
        _workers.emplace_back([this]() {
            WorkerThreadFunction();
        });

        TC_LOG_DEBUG("module.playerbot.database",
            "Started database worker thread {}", i);
    }
}

void BotDatabasePool::StopWorkerThreads()
{
    // Stop the I/O context
    if (_ioContext) {
        _ioContext->stop();
    }

    // Wait for all worker threads to finish
    for (auto& worker : _workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    _workers.clear();
}

void BotDatabasePool::WorkerThreadFunction()
{
    while (!_shutdown.load()) {
        try {
            // Process query queue
            ProcessQueryQueue();

            // Run I/O context
            if (_ioContext) {
                _ioContext->run_one();
            }

            // Small sleep to prevent busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        catch (std::exception const& e) {
            TC_LOG_ERROR("module.playerbot.database",
                "Exception in worker thread: {}", e.what());
        }
    }
}

void BotDatabasePool::ProcessQueryQueue()
{
    QueryRequest* request = nullptr;
    while (_queryQueue.pop(request)) {
        if (request) {
            ExecuteQueryRequest(*request);
            delete request;
        }
    }
}

void BotDatabasePool::ExecuteQueryRequest(QueryRequest const& request)
{
    auto startTime = std::chrono::steady_clock::now();

    // Check timeout
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        startTime - request.submitTime);
    if (elapsed.count() >= request.timeoutMs) {
        HandleQueryTimeout(request);
        return;
    }

    // Acquire connection
    size_t connectionIndex = AcquireConnection();
    if (connectionIndex == SIZE_MAX) {
        TC_LOG_WARN("module.playerbot.database",
            "No available connections for query {}", request.requestId);
        if (request.callback) request.callback(nullptr);
        return;
    }

    PreparedQueryResult result = nullptr;

    try {
        auto& connectionInfo = _connections[connectionIndex];
        if (connectionInfo && connectionInfo->connection) {
            // Execute query
            result = connectionInfo->connection->Query(request.statement);
            connectionInfo->queryCount++;

            // Cache the result if successful
            if (result) {
                std::string cacheKey = GenerateCacheKey(request.statement);
                CacheResult(cacheKey, result);
            }
        }
    }
    catch (std::exception const& e) {
        TC_LOG_ERROR("module.playerbot.database",
            "Exception executing query {}: {}", request.requestId, e.what());
        _metrics.errors.fetch_add(1, std::memory_order_relaxed);
    }

    // Release connection
    ReleaseConnection(connectionIndex);

    // Handle result
    HandleQueryResult(request, result);

    // Record metrics
    RecordQueryExecution(startTime);
}

void BotDatabasePool::HandleQueryResult(QueryRequest const& request, PreparedQueryResult result)
{
    if (request.callback) {
        try {
            request.callback(result);
        }
        catch (std::exception const& e) {
            TC_LOG_ERROR("module.playerbot.database",
                "Exception in query callback: {}", e.what());
        }
    }
}

void BotDatabasePool::EvictLeastRecentlyUsed()
{
    if (_resultCache.empty()) return;

    // Find least recently used entry
    auto oldestIt = _resultCache.begin();
    auto oldestTime = oldestIt->second.lastAccess;

    for (auto it = _resultCache.begin(); it != _resultCache.end(); ++it) {
        if (it->second.lastAccess < oldestTime) {
            oldestTime = it->second.lastAccess;
            oldestIt = it;
        }
    }

    _resultCache.erase(oldestIt);
}

std::string BotDatabasePool::GenerateCacheKey(PreparedStatement const* stmt) const
{
    if (!stmt) return "";

    std::ostringstream key;
    key << stmt->GetIndex();

    // Add parameter values to make key unique
    // This is a simplified implementation
    // A full implementation would hash the actual parameter values

    return key.str();
}

void BotDatabasePool::RecordQueryExecution(std::chrono::steady_clock::time_point startTime)
{
    auto endTime = std::chrono::steady_clock::now();
    uint32 responseTimeMs = static_cast<uint32>(
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());

    _metrics.queriesExecuted.fetch_add(1, std::memory_order_relaxed);

    // Update average response time (simplified moving average)
    uint32 currentAvg = _metrics.avgResponseTimeMs.load();
    uint32 newAvg = (currentAvg + responseTimeMs) / 2;
    _metrics.avgResponseTimeMs.store(newAvg, std::memory_order_relaxed);

    // Update max response time
    uint32 currentMax = _metrics.maxResponseTimeMs.load();
    if (responseTimeMs > currentMax) {
        _metrics.maxResponseTimeMs.store(responseTimeMs, std::memory_order_relaxed);
    }

    // Warn if response time exceeds target
    if (responseTimeMs > 10) {
        TC_LOG_DEBUG("module.playerbot.database",
            "Query response time {}ms exceeds target 10ms", responseTimeMs);
    }
}

void BotDatabasePool::HandleQueryTimeout(QueryRequest const& request)
{
    TC_LOG_WARN("module.playerbot.database",
        "Query {} timed out after {}ms", request.requestId, request.timeoutMs);

    _metrics.timeouts.fetch_add(1, std::memory_order_relaxed);

    if (request.callback) {
        request.callback(nullptr);
    }
}

} // namespace Playerbot