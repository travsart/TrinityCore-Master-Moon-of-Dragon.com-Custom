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
#include "MovementTypes.h"
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>
#include <queue>

class Player;
class Unit;
class Group;
class PathGenerator;
class GameObject;

// Movement namespace typedef removed to avoid conflict with TrinityCore
// Use Playerbot::PositionVector from core or G3D::Vector3 path conversion

namespace Playerbot
{
    // Forward declarations
    class MovementGenerator;
    class PathfindingAdapter;

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
    private:
        MovementManager();
        ~MovementManager();

        // Friend declarations to allow std::unique_ptr to access private constructor/destructor
        friend std::unique_ptr<MovementManager> std::make_unique<MovementManager>();
        friend struct std::default_delete<MovementManager>;

    public:
        static MovementManager* Instance();

        // Initialize and cleanup
        bool Initialize();
        void Shutdown();

        // Main update - called from BotAI
        void UpdateMovement(Player* bot, uint32 diff);

        // Basic movement commands (legacy interface - returns bool)
        bool MoveTo(Player* bot, Position const& dest, float speed = 0.0f);
        bool MoveTo(Player* bot, float x, float y, float z, float speed = 0.0f);
        bool MoveToUnit(Player* bot, Unit* target, float distance = 0.0f);
        bool Follow(Player* bot, Unit* leader, float minDist = 2.0f, float maxDist = 5.0f);
        bool Stop(Player* bot);
        bool Pause(Player* bot, uint32 durationMs = 0);
        bool Resume(Player* bot);

        // Advanced movement commands
        bool MoveToWithPriority(Player* bot, MovementRequest const& request);
        bool PatrolPath(Player* bot, std::vector<Position> const& waypoints);
        bool MoveInFormation(Player* bot, Group* group, uint8 position);

        // Movement state queries
        bool IsMoving(Player* bot) const;
        bool IsStopped(Player* bot) const;
        float GetDistanceToDestination(Player* bot) const;
        uint32 GetTimeToDestination(Player* bot) const;
        Position GetDestination(Player* bot) const;

        // Pathfinding
        bool CalculatePath(Player* bot, Position const& dest, Playerbot::PositionVector& path);
        bool HasValidPath(Player* bot) const;
        void ClearPath(Player* bot);
        Playerbot::PositionVector GetCurrentPath(Player* bot) const;

        // Generator management
        void SetMovementGenerator(Player* bot, MovementGeneratorType type);
        MovementGeneratorType GetCurrentGeneratorType(Player* bot) const;
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

        // Movement commands with MovementResult return type
        void UpdateAll(uint32 diff);
        MovementResult FollowUnit(Player* bot, Unit* target, float minDist, float maxDist, float angle = 0.0f);
        MovementResult FleeFrom(Player* bot, Unit* threat, float distance);
        MovementResult Chase(Player* bot, Unit* target, float range = 0.0f, float angle = 0.0f);
        MovementResult MoveToPoint(Player* bot, Position const& position, float speed = 0.0f);
        MovementResult WanderAround(Player* bot, float radius, uint32 duration = 0);
        MovementResult MoveInFormation(Player* bot, Unit* leader, FormationType formation, uint8 slot);
        MovementResult Patrol(Player* bot, std::vector<Position> const& waypoints, bool cyclic = true);
        void StopMovement(Player* bot, bool clearGenerators = false);
        MovementState const* GetMovementStatePtr(Player* bot) const;
        MovementGeneratorPtr GetCurrentGenerator(Player* bot) const;
        bool SetGroupFormation(Player* leader, std::vector<Player*> const& members, FormationType formation);
        void UpdateGroupMovement(Player* leader, std::vector<Player*> const& members, uint32 diff);
        bool MoveGroupToPosition(std::vector<Player*> const& members, Position const& destination, bool maintainFormation);
        void ResetMetrics();
        void SetMaxBotsPerUpdate(uint32 maxBots);
        void SetPerformanceMonitoring(bool enable);
        float GetCPUUsage() const;
        float GetMemoryUsage() const;

    private:
        struct BotMovementData
        {
            std::unique_ptr<MovementGenerator> generator;
            MovementGeneratorPtr currentGenerator; // Shared pointer version
            MovementGeneratorPtr pendingGenerator;
            std::vector<MovementGeneratorPtr> generatorHistory;

            Playerbot::PositionVector currentPath;
            Position destination;
            Position lastPosition;
            MovementState state;
            Playerbot::MovementPriority priority = Playerbot::MovementPriority::PRIORITY_NORMAL;
            Playerbot::TerrainType terrainType = Playerbot::TerrainType::TERRAIN_GROUND;
            float speed = 0.0f;
            float stopDistance = 0.0f;
            uint32 lastUpdate = 0;
            std::chrono::steady_clock::time_point lastUpdateTime;
            uint32 pathUpdateTimer = 0;
            uint32 stuckCheckTimer = 0;
            uint32 timeoutTimer = 0;
            uint32 pausedUntil = 0;
            bool isMoving = false;
            bool needsPathUpdate = false;
            bool needsUpdate = false;
            bool collisionEnabled = true;
            uint8 currentWaypoint = 0;
            uint8 stuckCounter = 0;

            // Path caching
            struct PathCache
            {
                Position destination;
                Playerbot::PositionVector path;
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
        Playerbot::PositionVector* GetCachedPath(BotMovementData& data, Position const& dest);
        void CachePath(BotMovementData& data, Position const& dest, Playerbot::PositionVector const& path);

        // Additional methods for MovementManager.cpp compatibility
        void ProcessUpdateQueue(uint32 maxUpdates);
        bool SwitchGenerator(Player* bot, MovementGeneratorPtr newGenerator);
        MovementGeneratorPtr CreateGenerator(MovementGeneratorType type, MovementPriority priority);
        void CleanupGenerators(Player* bot);
        void UpdatePerformanceMetrics(uint64 cpuTime, size_t memoryUsed);
        FormationPosition CalculateFormationPosition(FormationType formation, uint8 slot, uint8 totalSlots);
        bool LoadConfig();
        void ReloadConfig();

        // Thread-safe data access
        BotMovementData* GetMovementData(Player* bot);
        BotMovementData const* GetMovementData(Player* bot) const;

    private:
        // Bot movement data storage
        std::unordered_map<ObjectGuid, std::unique_ptr<BotMovementData>> m_botMovement;
        std::unordered_map<ObjectGuid, std::unique_ptr<BotMovementData>>& _botData = m_botMovement; // Alias for compatibility
        mutable std::shared_mutex m_mutex;

        // Subsystems
        std::unique_ptr<PathfindingAdapter> m_pathfinder;
        std::unique_ptr<PathfindingAdapter>& _pathfinder = m_pathfinder; // Alias for compatibility
        // TerrainAnalyzer removed - incomplete type, not implemented
        // GroupMovementCoordinator removed - incomplete type, not implemented
        // CollisionAvoidance removed - incomplete type, not implemented

        // Additional subsystems needed by cpp
        std::unique_ptr<class PathOptimizer> _optimizer;
        std::unique_ptr<class MovementValidator> _validator;
        std::unique_ptr<class NavMeshInterface> _navMesh;

        // Performance tracking
        std::atomic<uint32> m_activeMovements{0};
        std::atomic<uint32> m_lastPathComputeTime{0};
        std::atomic<uint32> m_totalPathComputations{0};
        std::atomic<uint32> m_totalPathCacheHits{0};
        std::atomic<uint64> _totalCpuMicros{0};
        std::atomic<uint64> _totalMemoryBytes{0};

        // Configuration
        MovementConfig m_config;
        uint32 _maxBotsPerUpdate{50};
        uint32 _defaultUpdateInterval{250};
        float _defaultFollowDistance{5.0f};
        float _defaultFleeDistance{20.0f};
        float _formationSpread{3.0f};
        bool _enablePathOptimization{true};
        bool _enableStuckDetection{true};
        uint32 _pathCacheSize{100};
        uint32 _pathCacheDuration{5000};
        bool _performanceMonitoring{true};

        // Metrics
        MovementMetrics _globalMetrics;
        std::chrono::steady_clock::time_point _lastMetricsUpdate;

        // Group formations
        struct GroupFormationData
        {
            ObjectGuid leaderGuid;
            FormationType formation;
            std::unordered_map<ObjectGuid, FormationPosition> positions;
            bool isActive{false};
        };
        std::unordered_map<uint32, GroupFormationData> _groupFormations;

        // Update queue
        std::priority_queue<std::pair<int32, ObjectGuid>> _updateQueue;

        // Singleton instance
        static std::unique_ptr<MovementManager> s_instance;
        static std::once_flag s_initFlag;
    };

    // Helper macros for movement manager access
    #define sMovementMgr MovementManager::Instance()
}

#endif // _PLAYERBOT_MOVEMENTMANAGER_H_