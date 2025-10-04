/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MovementManager.h"
#include "MovementGenerator.h"
#include "../Pathfinding/PathfindingAdapter.h"
#include "../Pathfinding/PathOptimizer.h"
#include "MovementValidator.h"
#include "../Pathfinding/NavMeshInterface.h"
#include "Player.h"
#include "Group.h"
#include "Log.h"
#include "Config.h"
#include <execution>

namespace PlayerBot
{
    // Static member initialization
    MovementManager* MovementManager::_instance = nullptr;
    std::mutex MovementManager::_instanceMutex;

    MovementManager::MovementManager()
        : _updateCounter(0), _totalCpuMicros(0), _totalMemoryBytes(0),
          _performanceMonitoring(true), _maxBotsPerUpdate(MovementConstants::MAX_BOTS_PER_UPDATE),
          _defaultUpdateInterval(MovementConstants::UPDATE_INTERVAL_NORMAL),
          _defaultFollowDistance(MovementConstants::FORMATION_FOLLOW_DIST),
          _defaultFleeDistance(20.0f), _formationSpread(MovementConstants::FORMATION_SPREAD),
          _enablePathOptimization(true), _enableStuckDetection(true),
          _pathCacheSize(MovementConstants::PATH_CACHE_SIZE),
          _pathCacheDuration(MovementConstants::PATH_CACHE_DURATION)
    {
        _lastMetricsUpdate = std::chrono::steady_clock::now();
        _globalMetrics.Reset();
    }

    MovementManager::~MovementManager()
    {
        Shutdown();
    }

    MovementManager* MovementManager::Instance()
    {
        if (!_instance)
        {
            std::lock_guard<std::mutex> lock(_instanceMutex);
            if (!_instance)
            {
                _instance = new MovementManager();
                _instance->Initialize();
            }
        }
        return _instance;
    }

    bool MovementManager::Initialize()
    {
        TC_LOG_INFO("playerbot.movement", "Initializing Movement Manager...");

        // Create subsystem components
        _pathfinder = std::make_unique<PathfindingAdapter>();
        _optimizer = std::make_unique<PathOptimizer>();
        _validator = std::make_unique<MovementValidator>();
        _navMesh = std::make_unique<NavMeshInterface>();

        // Initialize pathfinding
        if (!_pathfinder->Initialize(_pathCacheSize, _pathCacheDuration))
        {
            TC_LOG_ERROR("playerbot.movement", "Failed to initialize pathfinding adapter");
            return false;
        }

        // Initialize optimizer
        _optimizer->SetOptimizationLevel(_enablePathOptimization ? 2 : 0);

        // Initialize validator
        _validator->EnableStuckDetection(_enableStuckDetection);

        // Load configuration
        if (!LoadConfig())
        {
            TC_LOG_WARN("playerbot.movement", "Failed to load configuration, using defaults");
        }

        TC_LOG_INFO("playerbot.movement", "Movement Manager initialized successfully");
        return true;
    }

    void MovementManager::Shutdown()
    {
        TC_LOG_INFO("playerbot.movement", "Shutting down Movement Manager...");

        // Stop all active movements
        {
            std::unique_lock<std::shared_mutex> lock(_dataLock);
            for (auto& [guid, data] : _botData)
            {
                if (data->currentGenerator)
                {
                    data->currentGenerator->Finalize(nullptr, true);
                }
            }
            _botData.clear();
            _groupFormations.clear();
        }

        // Cleanup components
        _pathfinder.reset();
        _optimizer.reset();
        _validator.reset();
        _navMesh.reset();

        TC_LOG_INFO("playerbot.movement", "Movement Manager shutdown complete");
    }

    void MovementManager::UpdateMovement(Player* bot, uint32 diff)
    {
        if (!bot || !bot->IsInWorld())
            return;

        auto startTime = std::chrono::high_resolution_clock::now();

        BotMovementData* data = nullptr;
        {
            std::shared_lock<std::shared_mutex> lock(_dataLock);
            auto it = _botData.find(bot->GetGUID());
            if (it == _botData.end())
            {
                // Create new movement data if doesn't exist
                lock.unlock();
                std::unique_lock<std::shared_mutex> writeLock(_dataLock);
                auto [iter, inserted] = _botData.emplace(bot->GetGUID(),
                    std::make_unique<BotMovementData>());
                data = iter->second.get();
            }
            else
            {
                data = it->second.get();
            }
        }

        if (!data)
            return;

        // Check if update is needed based on priority and timing
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - data->lastUpdate).count();

        uint32 requiredInterval = _defaultUpdateInterval;
        if (bot->IsInCombat())
            requiredInterval = MovementConstants::UPDATE_INTERVAL_COMBAT;
        else if (!data->state.isMoving)
            requiredInterval = MovementConstants::UPDATE_INTERVAL_IDLE;

        if (elapsed < requiredInterval && !data->needsUpdate)
            return;

        data->lastUpdate = now;
        data->needsUpdate = false;

        // Process pending generator switch
        if (data->pendingGenerator)
        {
            if (SwitchGenerator(bot, data->pendingGenerator))
            {
                data->pendingGenerator.reset();
            }
        }

        // Update current generator
        if (data->currentGenerator && data->currentGenerator->IsActive())
        {
            MovementResult result = data->currentGenerator->Update(bot, diff);
            data->state.lastResult = result;

            switch (result)
            {
            case MovementResult::MOVEMENT_SUCCESS:
            case MovementResult::MOVEMENT_IN_PROGRESS:
                break;

            case MovementResult::MOVEMENT_FAILED:
            case MovementResult::MOVEMENT_UNREACHABLE:
            case MovementResult::MOVEMENT_NO_PATH:
                TC_LOG_DEBUG("playerbot.movement", "Movement failed for bot %s: %u",
                    bot->GetName().c_str(), static_cast<uint8>(result));
                data->currentGenerator->Finalize(bot, true);
                data->currentGenerator.reset();
                data->state.Reset();
                break;

            case MovementResult::MOVEMENT_STUCK:
                if (_validator->HandleStuck(bot))
                {
                    data->state.stuckCounter = 0;
                }
                else
                {
                    // Too stuck, give up
                    data->currentGenerator->Finalize(bot, true);
                    data->currentGenerator.reset();
                    data->state.Reset();
                }
                break;

            default:
                break;
            }
        }

        // Track performance
        if (_performanceMonitoring)
        {
            auto endTime = std::chrono::high_resolution_clock::now();
            uint64 cpuMicros = std::chrono::duration_cast<std::chrono::microseconds>(
                endTime - startTime).count();
            _totalCpuMicros.fetch_add(cpuMicros);

            // Update metrics
            if (cpuMicros > MovementConstants::PATH_GENERATION_BUDGET)
            {
                TC_LOG_DEBUG("playerbot.movement", "Movement update exceeded budget: %llu us for bot %s",
                    cpuMicros, bot->GetName().c_str());
            }
        }
    }

    void MovementManager::UpdateAll(uint32 diff)
    {
        if (_botData.empty())
            return;

        // Process update queue with load balancing
        ProcessUpdateQueue(_maxBotsPerUpdate);

        // Update metrics periodically
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(
            now - _lastMetricsUpdate).count() >= 1)
        {
            _globalMetrics.UpdateAverages();
            _lastMetricsUpdate = now;
        }
    }

    MovementResult MovementManager::Follow(Player* bot, Unit* target, float minDist,
                                          float maxDist, float angle)
    {
        if (!bot || !target)
            return MovementResult::MOVEMENT_FAILED;

        // Create follow generator (implementation would be in specific generator classes)
        auto generator = CreateGenerator(MovementGeneratorType::MOVEMENT_FOLLOW,
                                        MovementPriority::PRIORITY_NORMAL);
        if (!generator)
            return MovementResult::MOVEMENT_FAILED;

        // Initialize with parameters
        MovementRequest request;
        request.targetGuid = target->GetGUID();
        request.range = minDist;
        request.angle = angle;
        request.type = MovementGeneratorType::MOVEMENT_FOLLOW;

        // Switch to new generator
        if (!SwitchGenerator(bot, generator))
            return MovementResult::MOVEMENT_FAILED;

        return MovementResult::MOVEMENT_IN_PROGRESS;
    }

    MovementResult MovementManager::Flee(Player* bot, Unit* threat, float distance)
    {
        if (!bot || !threat)
            return MovementResult::MOVEMENT_FAILED;

        auto generator = CreateGenerator(MovementGeneratorType::MOVEMENT_FLEE,
                                        MovementPriority::PRIORITY_FLEE);
        if (!generator)
            return MovementResult::MOVEMENT_FAILED;

        MovementRequest request;
        request.targetGuid = threat->GetGUID();
        request.range = distance;
        request.type = MovementGeneratorType::MOVEMENT_FLEE;
        request.priority = MovementPriority::PRIORITY_FLEE;

        if (!SwitchGenerator(bot, generator))
            return MovementResult::MOVEMENT_FAILED;

        return MovementResult::MOVEMENT_IN_PROGRESS;
    }

    MovementResult MovementManager::Chase(Player* bot, Unit* target, float range, float angle)
    {
        if (!bot || !target)
            return MovementResult::MOVEMENT_FAILED;

        auto generator = CreateGenerator(MovementGeneratorType::MOVEMENT_CHASE,
                                        MovementPriority::PRIORITY_COMBAT);
        if (!generator)
            return MovementResult::MOVEMENT_FAILED;

        MovementRequest request;
        request.targetGuid = target->GetGUID();
        request.range = range;
        request.angle = angle;
        request.type = MovementGeneratorType::MOVEMENT_CHASE;
        request.priority = MovementPriority::PRIORITY_COMBAT;

        if (!SwitchGenerator(bot, generator))
            return MovementResult::MOVEMENT_FAILED;

        return MovementResult::MOVEMENT_IN_PROGRESS;
    }

    MovementResult MovementManager::MoveToPoint(Player* bot, Position const& position, float speed)
    {
        if (!bot)
            return MovementResult::MOVEMENT_FAILED;

        // Validate destination
        if (!_validator->ValidateDestination(bot, position))
            return MovementResult::MOVEMENT_INVALID_DEST;

        auto generator = CreateGenerator(MovementGeneratorType::MOVEMENT_POINT,
                                        MovementPriority::PRIORITY_NORMAL);
        if (!generator)
            return MovementResult::MOVEMENT_FAILED;

        MovementRequest request;
        request.destination = position;
        request.speed = speed > 0.0f ? speed : bot->GetSpeed(MOVE_RUN);
        request.type = MovementGeneratorType::MOVEMENT_POINT;

        if (!SwitchGenerator(bot, generator))
            return MovementResult::MOVEMENT_FAILED;

        return MovementResult::MOVEMENT_IN_PROGRESS;
    }

    MovementResult MovementManager::Wander(Player* bot, float radius, uint32 duration)
    {
        if (!bot)
            return MovementResult::MOVEMENT_FAILED;

        auto generator = CreateGenerator(MovementGeneratorType::MOVEMENT_RANDOM,
                                        MovementPriority::PRIORITY_NORMAL);
        if (!generator)
            return MovementResult::MOVEMENT_FAILED;

        // Wander parameters would be passed through the generator initialization
        if (!SwitchGenerator(bot, generator))
            return MovementResult::MOVEMENT_FAILED;

        return MovementResult::MOVEMENT_IN_PROGRESS;
    }

    MovementResult MovementManager::MoveInFormation(Player* bot, Unit* leader,
                                                   FormationType formation, uint8 slot)
    {
        if (!bot || !leader)
            return MovementResult::MOVEMENT_FAILED;

        // Calculate formation position
        FormationPosition formPos = CalculateFormationPosition(formation, slot, 1);

        auto generator = CreateGenerator(MovementGeneratorType::MOVEMENT_FORMATION,
                                        MovementPriority::PRIORITY_NORMAL);
        if (!generator)
            return MovementResult::MOVEMENT_FAILED;

        MovementRequest request;
        request.targetGuid = leader->GetGUID();
        request.type = MovementGeneratorType::MOVEMENT_FORMATION;

        if (!SwitchGenerator(bot, generator))
            return MovementResult::MOVEMENT_FAILED;

        // Store formation data
        {
            std::unique_lock<std::shared_mutex> lock(_dataLock);
            uint32 groupId = bot->GetGroup() ? bot->GetGroup()->GetGUID().GetCounter() : 0;
            if (groupId > 0)
            {
                _groupFormations[groupId].positions[bot->GetGUID()] = formPos;
                _groupFormations[groupId].leaderGuid = leader->GetGUID();
                _groupFormations[groupId].formation = formation;
                _groupFormations[groupId].isActive = true;
            }
        }

        return MovementResult::MOVEMENT_IN_PROGRESS;
    }

    MovementResult MovementManager::Patrol(Player* bot, std::vector<Position> const& waypoints,
                                          bool cyclic)
    {
        if (!bot || waypoints.empty())
            return MovementResult::MOVEMENT_FAILED;

        auto generator = CreateGenerator(MovementGeneratorType::MOVEMENT_PATROL,
                                        MovementPriority::PRIORITY_NORMAL);
        if (!generator)
            return MovementResult::MOVEMENT_FAILED;

        // Patrol waypoints would be passed through the generator
        if (!SwitchGenerator(bot, generator))
            return MovementResult::MOVEMENT_FAILED;

        return MovementResult::MOVEMENT_IN_PROGRESS;
    }

    void MovementManager::StopMovement(Player* bot, bool clearGenerators)
    {
        if (!bot)
            return;

        std::unique_lock<std::shared_mutex> lock(_dataLock);
        auto it = _botData.find(bot->GetGUID());
        if (it != _botData.end())
        {
            if (it->second->currentGenerator)
            {
                it->second->currentGenerator->Finalize(bot, true);
                if (clearGenerators)
                {
                    it->second->currentGenerator.reset();
                    it->second->pendingGenerator.reset();
                    it->second->generatorHistory.clear();
                }
            }
            it->second->state.Reset();
        }

        // Stop actual movement
        bot->StopMoving();
    }

    bool MovementManager::IsMoving(Player* bot) const
    {
        if (!bot)
            return false;

        std::shared_lock<std::shared_mutex> lock(_dataLock);
        auto it = _botData.find(bot->GetGUID());
        return it != _botData.end() && it->second->state.isMoving;
    }

    MovementState const* MovementManager::GetMovementState(Player* bot) const
    {
        if (!bot)
            return nullptr;

        std::shared_lock<std::shared_mutex> lock(_dataLock);
        auto it = _botData.find(bot->GetGUID());
        return it != _botData.end() ? &it->second->state : nullptr;
    }

    MovementGeneratorPtr MovementManager::GetCurrentGenerator(Player* bot) const
    {
        if (!bot)
            return nullptr;

        std::shared_lock<std::shared_mutex> lock(_dataLock);
        auto it = _botData.find(bot->GetGUID());
        return it != _botData.end() ? it->second->currentGenerator : nullptr;
    }

    bool MovementManager::SetGroupFormation(Player* leader, std::vector<Player*> const& members,
                                           FormationType formation)
    {
        if (!leader || members.empty())
            return false;

        std::unique_lock<std::shared_mutex> lock(_dataLock);

        uint32 groupId = leader->GetGroup() ? leader->GetGroup()->GetGUID().GetCounter() : 0;
        if (groupId == 0)
            return false;

        auto& groupData = _groupFormations[groupId];
        groupData.leaderGuid = leader->GetGUID();
        groupData.formation = formation;
        groupData.isActive = true;

        // Calculate positions for each member
        uint8 slot = 0;
        for (Player* member : members)
        {
            if (member != leader)
            {
                FormationPosition pos = CalculateFormationPosition(formation, slot++,
                    static_cast<uint8>(members.size() - 1));
                groupData.positions[member->GetGUID()] = pos;

                // Trigger formation movement for each bot
                MoveInFormation(member, leader, formation, slot - 1);
            }
        }

        return true;
    }

    void MovementManager::UpdateGroupMovement(Player* leader, std::vector<Player*> const& members,
                                             uint32 diff)
    {
        if (!leader || members.empty())
            return;

        uint32 groupId = leader->GetGroup() ? leader->GetGroup()->GetGUID().GetCounter() : 0;
        if (groupId == 0)
            return;

        std::shared_lock<std::shared_mutex> lock(_dataLock);
        auto it = _groupFormations.find(groupId);
        if (it == _groupFormations.end() || !it->second.isActive)
            return;

        auto& groupData = it->second;

        // Check if leader has moved significantly
        Position leaderPos = leader->GetPosition();
        for (Player* member : members)
        {
            if (member == leader)
                continue;

            auto posIt = groupData.positions.find(member->GetGUID());
            if (posIt != groupData.positions.end())
            {
                // Calculate desired position based on formation
                float angle = leaderPos.GetOrientation() + posIt->second.followAngle;
                float x = leaderPos.GetPositionX() + posIt->second.followDistance * cos(angle);
                float y = leaderPos.GetPositionY() + posIt->second.followDistance * sin(angle);
                float z = leaderPos.GetPositionZ();

                Position formationPos(x, y, z, angle);

                // Check if member needs to reposition
                float dist = member->GetExactDist(&formationPos);
                if (dist > MovementConstants::RECALC_THRESHOLD)
                {
                    lock.unlock();
                    MoveToPoint(member, formationPos);
                    lock.lock();
                }
            }
        }
    }

    bool MovementManager::MoveGroupToPosition(std::vector<Player*> const& members,
                                             Position const& destination, bool maintainFormation)
    {
        if (members.empty())
            return false;

        bool success = true;

        if (maintainFormation && members.size() > 1)
        {
            // Move in formation
            Player* leader = members[0];
            MoveToPoint(leader, destination);

            for (size_t i = 1; i < members.size(); ++i)
            {
                // Other members maintain formation relative to leader
                success &= (MoveInFormation(members[i], leader, FormationType::FORMATION_COLUMN,
                    static_cast<uint8>(i - 1)) == MovementResult::MOVEMENT_IN_PROGRESS);
            }
        }
        else
        {
            // Move independently
            for (Player* member : members)
            {
                success &= (MoveToPoint(member, destination) == MovementResult::MOVEMENT_IN_PROGRESS);
            }
        }

        return success;
    }

    void MovementManager::ResetMetrics()
    {
        _globalMetrics.Reset();
        _totalCpuMicros.store(0);
        _totalMemoryBytes.store(0);
    }

    void MovementManager::SetMaxBotsPerUpdate(uint32 maxBots)
    {
        _maxBotsPerUpdate = std::min(maxBots, static_cast<uint32>(1000));
    }

    void MovementManager::SetPerformanceMonitoring(bool enable)
    {
        _performanceMonitoring = enable;
    }

    float MovementManager::GetCPUUsage() const
    {
        uint64 totalMicros = _totalCpuMicros.load();
        // Calculate percentage based on expected frame time (100ms world update)
        return static_cast<float>(totalMicros) / 100000.0f;
    }

    float MovementManager::GetMemoryUsage() const
    {
        size_t totalBytes = _totalMemoryBytes.load();
        return static_cast<float>(totalBytes) / (1024 * 1024); // Convert to MB
    }

    bool MovementManager::LoadConfig()
    {
        _maxBotsPerUpdate = sConfigMgr->GetOption<uint32>("Playerbot.Movement.MaxBotsPerUpdate",
            MovementConstants::MAX_BOTS_PER_UPDATE);
        _defaultUpdateInterval = sConfigMgr->GetOption<uint32>("Playerbot.Movement.UpdateInterval",
            MovementConstants::UPDATE_INTERVAL_NORMAL);
        _defaultFollowDistance = sConfigMgr->GetOption<float>("Playerbot.Movement.FollowDistance",
            MovementConstants::FORMATION_FOLLOW_DIST);
        _defaultFleeDistance = sConfigMgr->GetOption<float>("Playerbot.Movement.FleeDistance", 20.0f);
        _formationSpread = sConfigMgr->GetOption<float>("Playerbot.Movement.FormationSpread",
            MovementConstants::FORMATION_SPREAD);
        _enablePathOptimization = sConfigMgr->GetOption<bool>("Playerbot.Movement.EnableOptimization", true);
        _enableStuckDetection = sConfigMgr->GetOption<bool>("Playerbot.Movement.EnableStuckDetection", true);
        _pathCacheSize = sConfigMgr->GetOption<uint32>("Playerbot.Movement.PathCacheSize",
            MovementConstants::PATH_CACHE_SIZE);
        _pathCacheDuration = sConfigMgr->GetOption<uint32>("Playerbot.Movement.PathCacheDuration",
            MovementConstants::PATH_CACHE_DURATION);

        return true;
    }

    void MovementManager::ReloadConfig()
    {
        LoadConfig();

        if (_pathfinder)
            _pathfinder->SetCacheParameters(_pathCacheSize, _pathCacheDuration);

        if (_optimizer)
            _optimizer->SetOptimizationLevel(_enablePathOptimization ? 2 : 0);

        if (_validator)
            _validator->EnableStuckDetection(_enableStuckDetection);
    }

    MovementGeneratorPtr MovementManager::CreateGenerator(MovementGeneratorType type,
                                                         MovementPriority priority)
    {
        // This would create specific generator instances based on type
        // For now, returning base generator as placeholder
        // Real implementation would instantiate FollowGenerator, FleeGenerator, etc.
        return std::make_shared<MovementGenerator>(type, priority);
    }

    bool MovementManager::SwitchGenerator(Player* bot, MovementGeneratorPtr newGenerator)
    {
        if (!bot || !newGenerator)
            return false;

        std::unique_lock<std::shared_mutex> lock(_dataLock);
        auto it = _botData.find(bot->GetGUID());
        if (it == _botData.end())
        {
            _botData[bot->GetGUID()] = std::make_unique<BotMovementData>();
            it = _botData.find(bot->GetGUID());
        }

        auto& data = it->second;

        // Check if current generator can be interrupted
        if (data->currentGenerator)
        {
            if (!data->currentGenerator->CanBeInterrupted(newGenerator->GetType(),
                newGenerator->GetPriority()))
            {
                // Queue for later if higher priority
                if (newGenerator->GetPriority() > data->currentGenerator->GetPriority())
                {
                    data->pendingGenerator = newGenerator;
                }
                return false;
            }

            // Finalize current generator
            data->currentGenerator->OnInterrupted(bot, newGenerator->GetType());
            data->currentGenerator->Finalize(bot, true);

            // Store in history
            if (data->generatorHistory.size() >= 10)
                data->generatorHistory.erase(data->generatorHistory.begin());
            data->generatorHistory.push_back(data->currentGenerator);
        }

        // Initialize new generator
        if (!newGenerator->Initialize(bot))
        {
            TC_LOG_ERROR("playerbot.movement", "Failed to initialize movement generator type %u for bot %s",
                static_cast<uint8>(newGenerator->GetType()), bot->GetName().c_str());
            return false;
        }

        data->currentGenerator = newGenerator;
        data->state.currentType = newGenerator->GetType();
        data->needsUpdate = true;

        return true;
    }

    void MovementManager::CleanupGenerators(Player* bot)
    {
        if (!bot)
            return;

        std::unique_lock<std::shared_mutex> lock(_dataLock);
        auto it = _botData.find(bot->GetGUID());
        if (it != _botData.end())
        {
            // Keep only recent history
            while (it->second->generatorHistory.size() > 5)
            {
                it->second->generatorHistory.erase(it->second->generatorHistory.begin());
            }
        }
    }

    void MovementManager::UpdatePerformanceMetrics(uint64 cpuTime, size_t memoryUsed)
    {
        _totalCpuMicros.fetch_add(cpuTime);
        _totalMemoryBytes.store(memoryUsed);

        // Update global metrics
        _globalMetrics.totalCpuMicros += cpuTime;
    }

    void MovementManager::ProcessUpdateQueue(uint32 maxUpdates)
    {
        uint32 processed = 0;

        while (!_updateQueue.empty() && processed < maxUpdates)
        {
            auto [priority, guid] = _updateQueue.top();
            _updateQueue.pop();

            // Find bot by GUID and update
            // This would need integration with actual bot management system
            processed++;
        }
    }

    FormationPosition MovementManager::CalculateFormationPosition(FormationType formation,
                                                                 uint8 slot, uint8 totalSlots)
    {
        FormationPosition pos;
        float spread = _formationSpread;

        switch (formation)
        {
        case FormationType::FORMATION_LINE:
            pos.relativeX = static_cast<float>(slot - totalSlots / 2) * spread;
            pos.relativeY = 0.0f;
            break;

        case FormationType::FORMATION_COLUMN:
            pos.relativeX = 0.0f;
            pos.relativeY = -static_cast<float>(slot + 1) * spread;
            break;

        case FormationType::FORMATION_WEDGE:
            {
                uint8 row = static_cast<uint8>(sqrt(slot));
                uint8 col = slot - (row * row);
                pos.relativeX = static_cast<float>(col - row) * spread;
                pos.relativeY = -static_cast<float>(row) * spread;
            }
            break;

        case FormationType::FORMATION_CIRCLE:
            {
                float angle = (2 * M_PI * slot) / totalSlots;
                float radius = spread * totalSlots / (2 * M_PI);
                pos.relativeX = radius * cos(angle);
                pos.relativeY = radius * sin(angle);
            }
            break;

        case FormationType::FORMATION_SQUARE:
            {
                uint8 side = static_cast<uint8>(sqrt(totalSlots)) + 1;
                uint8 row = slot / side;
                uint8 col = slot % side;
                pos.relativeX = static_cast<float>(col - side / 2) * spread;
                pos.relativeY = -static_cast<float>(row) * spread;
            }
            break;

        default:
            pos.relativeX = 0.0f;
            pos.relativeY = -static_cast<float>(slot + 1) * spread;
            break;
        }

        pos.relativeAngle = atan2(pos.relativeY, pos.relativeX);
        pos.followDistance = sqrt(pos.relativeX * pos.relativeX + pos.relativeY * pos.relativeY);
        pos.followAngle = pos.relativeAngle;
        pos.slot = slot;

        return pos;
    }
}