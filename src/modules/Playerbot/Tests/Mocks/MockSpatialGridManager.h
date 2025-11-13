/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 *
 * Mock implementation of ISpatialGridManager for unit testing
 */

#pragma once

#include "Core/DI/Interfaces/ISpatialGridManager.h"
#include <unordered_map>
#include <vector>

class Map;

namespace Playerbot::Testing
{

/**
 * @brief Mock Spatial Grid Manager for unit testing
 *
 * Provides a simple in-memory implementation of ISpatialGridManager
 * that can be used to test code that depends on spatial grids without
 * requiring real Map instances or spatial grid infrastructure.
 *
 * **Features:**
 * - No-op implementations for CreateGrid/DestroyGrid
 * - Configurable return values for GetGrid
 * - Call tracking for verification
 * - Lightweight and fast for unit tests
 *
 * **Usage:**
 * @code
 * TEST_CASE("BotAI uses spatial grid")
 * {
 *     auto mockSpatialMgr = std::make_shared<MockSpatialGridManager>();
 *
 *     // Register mock in DI container
 *     Services::Container().RegisterInstance<ISpatialGridManager>(mockSpatialMgr);
 *
 *     // Create bot AI (will use mock spatial manager)
 *     BotAI ai(...);
 *     ai.FindNearestEnemy();
 *
 *     // Verify spatial grid was accessed
 *     REQUIRE(mockSpatialMgr->WasGetGridCalled());
 * }
 * @endcode
 */
class MockSpatialGridManager final : public ISpatialGridManager
{
public:
    MockSpatialGridManager() = default;
    ~MockSpatialGridManager() override = default;

    // ISpatialGridManager interface
    void CreateGrid(Map* map) override
    {
        _createGridCalls++;
    }

    void DestroyGrid(uint32 mapId) override
    {
        _destroyGridCalls++;
        _grids.erase(mapId);
    }

    DoubleBufferedSpatialGrid* GetGrid(uint32 mapId) override
    {
        _getGridCalls++;
        _lastQueriedMapId = mapId;

        auto it = _grids.find(mapId);
        return it != _grids.end() ? it->second : nullptr;
    }

    DoubleBufferedSpatialGrid* GetGrid(Map* map) override
    {
        // For mock, just return nullptr
        _getGridCalls++;
        return nullptr;
    }

    void DestroyAllGrids() override
    {
        _destroyAllGridsCalls++;
        _grids.clear();
    }

    void UpdateGrid(uint32 mapId) override
    {
        _updateGridCalls++;
    }

    void UpdateGrid(Map* map) override
    {
        _updateGridCalls++;
    }

    size_t GetGridCount() const override
    {
        return _grids.size();
    }

    // Mock-specific methods for test verification

    /**
     * @brief Set mock grid for testing
     *
     * @param mapId Map ID
     * @param grid Pointer to mock grid (or nullptr)
     */
    void SetMockGrid(uint32 mapId, DoubleBufferedSpatialGrid* grid)
    {
        _grids[mapId] = grid;
    }

    /**
     * @brief Check if GetGrid was called
     */
    bool WasGetGridCalled() const { return _getGridCalls > 0; }

    /**
     * @brief Get number of GetGrid calls
     */
    uint32 GetGetGridCallCount() const { return _getGridCalls; }

    /**
     * @brief Get last map ID queried with GetGrid
     */
    uint32 GetLastQueriedMapId() const { return _lastQueriedMapId; }

    /**
     * @brief Reset all call counters
     */
    void ResetCallCounters()
    {
        _createGridCalls = 0;
        _destroyGridCalls = 0;
        _getGridCalls = 0;
        _updateGridCalls = 0;
        _destroyAllGridsCalls = 0;
        _lastQueriedMapId = 0;
    }

private:
    // Mock data
    std::unordered_map<uint32, DoubleBufferedSpatialGrid*> _grids;

    // Call tracking
    uint32 _createGridCalls = 0;
    uint32 _destroyGridCalls = 0;
    uint32 _getGridCalls = 0;
    uint32 _updateGridCalls = 0;
    uint32 _destroyAllGridsCalls = 0;
    uint32 _lastQueriedMapId = 0;
};

/**
 * @brief Mock Bot Session Manager for unit testing
 *
 * Provides a simple in-memory implementation of IBotSessionMgr.
 */
class MockBotSessionMgr final : public IBotSessionMgr
{
public:
    MockBotSessionMgr() = default;
    ~MockBotSessionMgr() override = default;

    bool Initialize() override
    {
        _initialized = true;
        return true;
    }

    void Shutdown() override
    {
        _initialized = false;
        _sessions.clear();
    }

    BotSession* CreateSession(uint32 bnetAccountId) override
    {
        _createSessionCalls++;
        // Return mock session (nullptr for now - can be enhanced)
        return nullptr;
    }

    BotSession* CreateSession(uint32 bnetAccountId, ObjectGuid characterGuid) override
    {
        _createSessionCalls++;
        return nullptr;
    }

    BotSession* CreateAsyncSession(uint32 bnetAccountId, ObjectGuid characterGuid) override
    {
        _createAsyncSessionCalls++;
        return nullptr;
    }

    void ReleaseSession(uint32 bnetAccountId) override
    {
        _releaseSessionCalls++;
        _sessions.erase(bnetAccountId);
    }

    BotSession* GetSession(uint32 bnetAccountId) const override
    {
        _getSessionCalls++;
        auto it = _sessions.find(bnetAccountId);
        return it != _sessions.end() ? it->second : nullptr;
    }

    void UpdateAllSessions(uint32 diff) override
    {
        _updateAllSessionsCalls++;
        _lastDiff = diff;
    }

    bool IsEnabled() const override { return _enabled; }
    void SetEnabled(bool enabled) override { _enabled = enabled; }
    uint32 GetActiveSessionCount() const override { return static_cast<uint32>(_sessions.size()); }
    void TriggerCharacterLoginForAllSessions() override { _triggerLoginCalls++; }

    // Mock-specific methods
    bool WasCreateSessionCalled() const { return _createSessionCalls > 0; }
    uint32 GetCreateSessionCallCount() const { return _createSessionCalls; }
    bool WasUpdateAllSessionsCalled() const { return _updateAllSessionsCalls > 0; }
    uint32 GetLastDiff() const { return _lastDiff; }

    void ResetCallCounters()
    {
        _createSessionCalls = 0;
        _createAsyncSessionCalls = 0;
        _releaseSessionCalls = 0;
        _getSessionCalls = 0;
        _updateAllSessionsCalls = 0;
        _triggerLoginCalls = 0;
        _lastDiff = 0;
    }

private:
    mutable std::unordered_map<uint32, BotSession*> _sessions;
    bool _initialized = false;
    bool _enabled = true;

    // Call tracking
    mutable uint32 _createSessionCalls = 0;
    uint32 _createAsyncSessionCalls = 0;
    uint32 _releaseSessionCalls = 0;
    mutable uint32 _getSessionCalls = 0;
    uint32 _updateAllSessionsCalls = 0;
    uint32 _triggerLoginCalls = 0;
    uint32 _lastDiff = 0;
};

} // namespace Playerbot::Testing
