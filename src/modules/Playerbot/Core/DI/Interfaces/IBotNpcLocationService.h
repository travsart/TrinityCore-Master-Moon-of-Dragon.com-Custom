/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include <vector>

// Forward declarations
class Player;

namespace Playerbot
{

struct NpcLocationResult;
enum class NpcServiceType : uint8;

/**
 * @brief Interface for NPC location resolution service
 *
 * Provides fast, cached NPC location lookups for quest objectives,
 * trainers, vendors, and services with O(1) map-indexed performance.
 */
class IBotNpcLocationService
{
public:
    virtual ~IBotNpcLocationService() = default;

    // Lifecycle
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;

    // Quest objective location queries
    virtual NpcLocationResult FindQuestObjectiveLocation(Player* bot, uint32 questId, uint32 objectiveIndex) = 0;

    // Trainer location queries
    virtual NpcLocationResult FindNearestProfessionTrainer(Player* bot, uint32 skillId) = 0;
    virtual NpcLocationResult FindNearestClassTrainer(Player* bot, uint8 classId) = 0;

    // Service location queries
    virtual NpcLocationResult FindNearestService(Player* bot, NpcServiceType serviceType) = 0;

    // Generic spawn location queries
    virtual NpcLocationResult FindNearestCreatureSpawn(Player* bot, uint32 creatureEntry, float maxRange = 500.0f) = 0;
    virtual NpcLocationResult FindNearestGameObjectSpawn(Player* bot, uint32 objectEntry, float maxRange = 500.0f) = 0;

    // Status
    virtual bool IsInitialized() const = 0;

    // Cache building
    virtual void BuildCreatureSpawnCache() = 0;
    virtual void BuildGameObjectSpawnCache() = 0;
    virtual void BuildProfessionTrainerCache() = 0;
    virtual void BuildClassTrainerCache() = 0;
    virtual void BuildServiceNpcCache() = 0;
    virtual void BuildQuestPOICache() = 0;

    // Utility queries
    virtual bool IsTrainerForSkill(uint32 creatureEntry, uint32 skillId) = 0;
    virtual bool IsClassTrainer(uint32 creatureEntry, uint8 classId) = 0;
    virtual bool ProvidesService(uint32 creatureEntry, NpcServiceType serviceType) = 0;

    // Live entity finding
    virtual NpcLocationResult TryFindLiveCreature(Player* bot, uint32 creatureEntry, float maxRange) = 0;
    virtual NpcLocationResult TryFindLiveGameObject(Player* bot, uint32 objectEntry, float maxRange) = 0;
};

} // namespace Playerbot
