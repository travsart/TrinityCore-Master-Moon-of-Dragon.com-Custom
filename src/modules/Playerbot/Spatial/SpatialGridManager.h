/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#ifndef PLAYERBOT_SPATIAL_GRID_MANAGER_H
#define PLAYERBOT_SPATIAL_GRID_MANAGER_H

#include "Define.h"
#include "DoubleBufferedSpatialGrid.h"
#include "Threading/LockHierarchy.h"
#include "Core/DI/Interfaces/ISpatialGridManager.h"
#include <unordered_map>
#include <memory>
#include <shared_mutex>

class Map;

namespace Playerbot
{

/**
 * @brief Global manager for all spatial grids across all maps
 *
 * Implements ISpatialGridManager for dependency injection compatibility.
 * Uses singleton pattern for backward compatibility (transitional).
 *
 * **Migration Path:**
 * - Old code: sSpatialGridManager.GetGrid(mapId)
 * - New code: Services::Container().Resolve<ISpatialGridManager>()->GetGrid(mapId)
 */
class TC_GAME_API SpatialGridManager final : public ISpatialGridManager
{
public:
    static SpatialGridManager& Instance()
    {
        static SpatialGridManager instance;
        return instance;
    }

    // ISpatialGridManager interface implementation
    void CreateGrid(Map* map) override;
    void DestroyGrid(uint32 mapId) override;
    DoubleBufferedSpatialGrid* GetGrid(uint32 mapId) override;
    DoubleBufferedSpatialGrid* GetGrid(Map* map) override;
    void DestroyAllGrids() override;
    void UpdateGrid(uint32 mapId) override;
    void UpdateGrid(Map* map) override;
    size_t GetGridCount() const override;

private:
    SpatialGridManager() = default;
    ~SpatialGridManager() { DestroyAllGrids(); }

    std::unordered_map<uint32, std::unique_ptr<DoubleBufferedSpatialGrid>> _grids;
    mutable OrderedSharedMutex<LockOrder::SPATIAL_GRID> _mutex;  // Allow concurrent reads with deadlock prevention

    // Non-copyable
    SpatialGridManager(SpatialGridManager const&) = delete;
    SpatialGridManager& operator=(SpatialGridManager const&) = delete;
};

// Convenience accessor
#define sSpatialGridManager Playerbot::SpatialGridManager::Instance()

} // namespace Playerbot

#endif // PLAYERBOT_SPATIAL_GRID_MANAGER_H
