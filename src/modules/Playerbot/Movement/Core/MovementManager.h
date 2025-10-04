/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef _PLAYERBOT_MOVEMENTMANAGER_H_
#define _PLAYERBOT_MOVEMENTMANAGER_H_

#include "Define.h"
#include "Position.h"
#include "ObjectGuid.h"
#include "MovementDefines.h"
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>
#include <queue>

class Player;
class Unit;
class Group;
class PathGenerator;

namespace Movement
{
    typedef std::vector<Position> PointsArray;
}

namespace Playerbot
{
    // Forward declarations
    class MovementGenerator;
    class PathfindingAdapter;
    class TerrainAnalyzer;
    class GroupMovementCoordinator;
    class CollisionAvoidance;

    enum class MovementGeneratorType : uint8
    {
        IDLE            = 0,
        POINT           = 1,
        CHASE           = 2,
        FOLLOW          = 3,
        FLEE            = 4,
        WANDER          = 5,
        FORMATION       = 6,
        PATROL          = 7,
        CUSTOM          = 8
    };

    enum class MovementPriority : uint8
    {
        NORMAL          = 0,
        COMBAT          = 1,
        QUEST           = 2,
        FORMATION       = 3,
        EMERGENCY       = 4
    };

    enum class MovementState : uint8
    {
        STOPPED         = 0,
        MOVING          = 1,
        PAUSED          = 2,
        ARRIVING        = 3,
        FAILED          = 4
    };

    enum class TerrainType : uint8
    {
        GROUND          = 0,
        WATER           = 1,
        UNDERWATER      = 2,
        FLYING          = 3,
        INDOOR          = 4,
        MOUNTED         = 5
    };

    struct MovementRequest
    {
        Position destination;
        float speed = 0.0f;
        float stopDistance = 0.0f;
        MovementPriority priority = MovementPriority::NORMAL;
        uint32 timeoutMs = 30000;
        bool allowPartialPath = false;
        bool forceDirect = false;
    };

    struct MovementConfig
    {
        static constexpr uint32 UPDATE_INTERVAL_MS = 100;
        static constexpr uint32 PATH_UPDATE_INTERVAL_MS = 500;
        static constexpr uint32 COLLISION_CHECK_INTERVAL_MS = 200;
        static constexpr float MIN_MOVE_DISTANCE = 0.5f;
        static constexpr float ARRIVAL_DISTANCE = 2.0f;
        static constexpr float COLLISION_RADIUS = 0.5f;
        static constexpr uint32 MAX_PATH_LENGTH = 200;
        static constexpr uint32 PATH_CACHE_SIZE = 10;
    };

    class MovementManager final
    {
        MovementManager();
        ~MovementManager();

    public:
        static MovementManager* Instance();

        // Initialize and cleanup
        bool Initialize();
        void Shutdown();

        // Main update - called from BotAI
        void UpdateMovement(Player* bot, uint32 diff);

        // Basic movement commands
        bool MoveTo(Player* bot, Position const& dest, float speed = 0.0f);
        bool MoveTo(Player* bot, float x, float y, float z, float speed = 0.0f);
        bool MoveToUnit(Player* bot, Unit* target, float distance = 0.0f);
        bool Follow(Player* bot, Unit* leader, float minDist = 2.0f, float maxDist = 5.0f);
        bool Flee(Player* bot, Unit* threat, float distance = 20.0f);
        bool Stop(Player* bot);
        bool Pause(Player* bot, uint32 durationMs = 0);
        bool Resume(Player* bot);

        // Advanced movement commands
        bool MoveToWithPriority(Player* bot, MovementRequest const& request);
        bool PatrolPath(Player* bot, std::vector<Position> const& waypoints);
        bool Wander(Player* bot, float radius = 10.0f);
        bool MoveInFormation(Player* bot, Group* group, uint8 position);

        // Movement state queries
        bool IsMoving(Player* bot) const;
        bool IsStopped(Player* bot) const;
        MovementState GetMovementState(Player* bot) const;
        float GetDistanceToDestination(Player* bot) const;
        uint32 GetTimeToDestination(Player* bot) const;
        Position GetDestination(Player* bot) const;

        // Pathfinding
        bool CalculatePath(Player* bot, Position const& dest, Movement::PointsArray& path);
        bool HasValidPath(Player* bot) const;
        void ClearPath(Player* bot);
        Movement::PointsArray GetCurrentPath(Player* bot) const;

        // Generator management
        void SetMovementGenerator(Player* bot, MovementGeneratorType type);
        MovementGeneratorType GetCurrentGenerator(Player* bot) const;
        MovementGenerator* GetGenerator(Player* bot) const;

        // Terrain handling
        TerrainType GetTerrainType(Player* bot) const;
        bool CanSwim(Player* bot) const;
        bool CanFly(Player* bot) const;
        bool IsIndoors(Player* bot) const;
        float GetSwimSpeed(Player* bot) const;
        float GetFlightSpeed(Player* bot) const;

        // Group coordination
        void UpdateFormation(Group* group, uint32 formationType);
        void SynchronizeGroupMovement(Group* group);
        bool IsInFormation(Player* bot) const;
        Position GetFormationPosition(Player* bot, Group* group) const;

        // Collision avoidance
        bool CheckCollision(Player* bot, Position const& dest) const;
        void EnableCollisionAvoidance(Player* bot, bool enable = true);
        void SetCollisionRadius(Player* bot, float radius);

        // Transport handling
        bool BoardTransport(Player* bot, GameObject* transport);
        bool ExitTransport(Player* bot);
        bool IsOnTransport(Player* bot) const;

        // Performance monitoring
        uint32 GetPathComputeTime() const { return m_lastPathComputeTime; }
        uint32 GetActiveMovementCount() const { return m_activeMovements; }
        void GetPerformanceMetrics(uint32& avgUpdateTime, uint32& pathCacheHits) const;

    private:
        struct BotMovementData
        {
            std::unique_ptr<MovementGenerator> generator;
            Movement::PointsArray currentPath;
            Position destination;
            Position lastPosition;
            MovementState state = MovementState::STOPPED;
            MovementPriority priority = MovementPriority::NORMAL;
            TerrainType terrainType = TerrainType::GROUND;
            float speed = 0.0f;
            float stopDistance = 0.0f;
            uint32 lastUpdate = 0;
            uint32 pathUpdateTimer = 0;
            uint32 stuckCheckTimer = 0;
            uint32 timeoutTimer = 0;
            uint32 pausedUntil = 0;
            bool isMoving = false;
            bool needsPathUpdate = false;
            bool collisionEnabled = true;
            uint8 currentWaypoint = 0;
            uint8 stuckCounter = 0;

            // Path caching
            struct PathCache
            {
                Position destination;
                Movement::PointsArray path;
                uint32 timestamp;
            };
            std::deque<PathCache> pathCache;

            // Performance metrics
            uint32 pathComputations = 0;
            uint32 pathCacheHits = 0;
            uint32 totalUpdateTime = 0;
        };

        // Internal methods
        void InitializeBotMovement(Player* bot);
        void CleanupBotMovement(Player* bot);
        void UpdateBotPosition(Player* bot, BotMovementData& data, uint32 diff);
        void UpdatePath(Player* bot, BotMovementData& data);
        void CheckStuck(Player* bot, BotMovementData& data);
        void HandleArrival(Player* bot, BotMovementData& data);
        bool ValidateMovement(Player* bot, Position const& dest) const;
        void ApplyTerrainAdjustments(Player* bot, BotMovementData& data);
        void SendMovementPacket(Player* bot, Position const& dest, float speed);
        Movement::PointsArray* GetCachedPath(BotMovementData& data, Position const& dest);
        void CachePath(BotMovementData& data, Position const& dest, Movement::PointsArray const& path);

        // Thread-safe data access
        BotMovementData* GetMovementData(Player* bot);
        BotMovementData const* GetMovementData(Player* bot) const;

    private:
        // Bot movement data storage
        std::unordered_map<ObjectGuid, std::unique_ptr<BotMovementData>> m_botMovement;
        mutable std::shared_mutex m_mutex;

        // Subsystems
        std::unique_ptr<PathfindingAdapter> m_pathfinder;
        std::unique_ptr<TerrainAnalyzer> m_terrainAnalyzer;
        std::unique_ptr<GroupMovementCoordinator> m_groupCoordinator;
        std::unique_ptr<CollisionAvoidance> m_collisionAvoidance;

        // Performance tracking
        std::atomic<uint32> m_activeMovements{0};
        std::atomic<uint32> m_lastPathComputeTime{0};
        std::atomic<uint32> m_totalPathComputations{0};
        std::atomic<uint32> m_totalPathCacheHits{0};

        // Configuration
        MovementConfig m_config;

        // Singleton instance
        static std::unique_ptr<MovementManager> s_instance;
        static std::once_flag s_initFlag;
    };

    // Helper macros for movement manager access
    #define sMovementMgr MovementManager::Instance()
}

#endif // _PLAYERBOT_MOVEMENTMANAGER_H_