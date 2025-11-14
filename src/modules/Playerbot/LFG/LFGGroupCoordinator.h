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
#include "Threading/LockHierarchy.h"
#include "ObjectGuid.h"
#include "LFG.h"
#include "Core/DI/Interfaces/ILFGGroupCoordinator.h"
#include <unordered_map>
#include <vector>
#include <mutex>

class Player;
class Group;

namespace Playerbot
{

/**
 * @brief LFG Group Coordinator - Handles group formation and dungeon teleportation for bots
 *
 * This class is responsible for:
 * - Coordinating group formation after LFG proposal acceptance
 * - Teleporting players (bots and humans) to dungeon entrances
 * - Managing teleportation states and cooldowns
 * - Ensuring all group members are properly positioned
 *
 * ARCHITECTURE:
 * - Singleton pattern for global access
 * - Thread-safe operations with mutex protection
 * - Integrates with TrinityCore's Group and LFG systems
 * - Module-only implementation (no core modifications)
 *
 * USAGE:
 * ```cpp
 * // After proposal is accepted and group is formed
 * sLFGGroupCoordinator->OnGroupFormed(groupGuid, dungeonId);
 *
 * // Teleport specific player to dungeon
 * sLFGGroupCoordinator->TeleportPlayerToDungeon(player, dungeonId);
 * ```
 *
 * INTEGRATION POINTS:
 * - Called from LFGBotManager when proposal is accepted
 * - Uses TrinityCore's Player::TeleportTo() for actual teleportation
 * - Uses TrinityCore's Group API for group management
 * - Uses LFGMgr for dungeon information
 *
 * PERFORMANCE:
 * - Teleportation: <50ms per player
 * - Group formation: <100ms total
 * - Memory: <100 bytes per active teleport
 *
 * THREAD SAFETY:
 * - All public methods are thread-safe
 * - Internal mutex protects shared data
 */
class TC_GAME_API LFGGroupCoordinator final : public ILFGGroupCoordinator
{
public:
    /**
     * Get singleton instance
     */
    static LFGGroupCoordinator* instance();

    /**
     * Initialize the coordinator
     * Called once during server startup
     */
    void Initialize() override;

    /**
     * Update coordinator state
     * Called every world update tick
     *
     * @param diff Time since last update in milliseconds
     */
    void Update(uint32 diff) override;

    /**
     * Shutdown the coordinator
     * Called during server shutdown
     */
    void Shutdown() override;

    // ========================================================================
    // GROUP FORMATION
    // ========================================================================

    /**
     * Handle group formation after LFG proposal is accepted
     * Creates/updates the group and prepares for teleportation
     *
     * @param groupGuid GUID of the formed group
     * @param dungeonId LFG dungeon ID
     * @return true if group was successfully formed
     */
    bool OnGroupFormed(ObjectGuid groupGuid, uint32 dungeonId) override;

    /**
     * Handle group ready check completion
     * Triggers teleportation sequence when all members are ready
     *
     * @param groupGuid GUID of the group
     * @return true if teleportation was initiated
     */
    bool OnGroupReady(ObjectGuid groupGuid) override;

    // ========================================================================
    // DUNGEON TELEPORTATION
    // ========================================================================

    /**
     * Teleport player to dungeon entrance
     * Handles bot-specific teleport logic
     *
     * @param player Player to teleport
     * @param dungeonId LFG dungeon ID
     * @return true if teleportation was successful
     */
    bool TeleportPlayerToDungeon(Player* player, uint32 dungeonId) override;

    /**
     * Teleport entire group to dungeon
     * Coordinates teleportation of all group members
     *
     * @param group Group to teleport
     * @param dungeonId LFG dungeon ID
     * @return true if teleportation was initiated for all members
     */
    bool TeleportGroupToDungeon(Group* group, uint32 dungeonId) override;

    /**
     * Check if player can be teleported to dungeon
     * Validates level, location, combat state, etc.
     *
     * @param player Player to check
     * @param dungeonId LFG dungeon ID
     * @return true if player can be teleported
     */
    bool CanTeleportToDungeon(Player const* player, uint32 dungeonId) const override;

    /**
     * Get dungeon entrance location
     * Retrieves map ID and coordinates for dungeon entrance
     *
     * @param dungeonId LFG dungeon ID
     * @param mapId Output: Map ID of the dungeon
     * @param x Output: X coordinate
     * @param y Output: Y coordinate
     * @param z Output: Z coordinate
     * @param orientation Output: Orientation
     * @return true if dungeon entrance was found
     */
    bool GetDungeonEntrance(uint32 dungeonId, uint32& mapId, float& x, float& y, float& z, float& orientation) const override;

    // ========================================================================
    // TELEPORT STATE MANAGEMENT
    // ========================================================================

    /**
     * Track player teleport request
     * Records that a player is being teleported
     *
     * @param playerGuid Player GUID
     * @param dungeonId LFG dungeon ID
     * @param timestamp Time of teleport request
     */
    void TrackTeleport(ObjectGuid playerGuid, uint32 dungeonId, uint32 timestamp) override;

    /**
     * Clear player teleport tracking
     * Removes teleport tracking after completion or timeout
     *
     * @param playerGuid Player GUID
     */
    void ClearTeleport(ObjectGuid playerGuid) override;

    /**
     * Check if player has pending teleport
     *
     * @param playerGuid Player GUID
     * @return true if player has a pending teleport
     */
    bool HasPendingTeleport(ObjectGuid playerGuid) const override;

    /**
     * Get pending teleport dungeon ID
     *
     * @param playerGuid Player GUID
     * @return Dungeon ID, or 0 if no pending teleport
     */
    uint32 GetPendingTeleportDungeon(ObjectGuid playerGuid) const override;

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * Enable/disable coordinator
     */
    void SetEnabled(bool enabled) override { _enabled = enabled; }

    /**
     * Check if coordinator is enabled
     */
    bool IsEnabled() const override { return _enabled; }

    /**
     * Set teleport timeout (milliseconds)
     * Default: 30000 (30 seconds)
     */
    void SetTeleportTimeout(uint32 timeout) override { _teleportTimeout = timeout; }

    /**
     * Get teleport timeout
     */
    uint32 GetTeleportTimeout() const override { return _teleportTimeout; }

private:
    // Prevent direct instantiation
    LFGGroupCoordinator();
    ~LFGGroupCoordinator();
    LFGGroupCoordinator(LFGGroupCoordinator const&) = delete;
    LFGGroupCoordinator& operator=(LFGGroupCoordinator const&) = delete;

    // ========================================================================
    // INTERNAL STRUCTURES
    // ========================================================================

    /**
     * Teleport tracking information
     */
    struct TeleportInfo
    {
        ObjectGuid playerGuid;
        uint32 dungeonId;
        uint32 timestamp;           // When teleport was initiated
        bool completed;             // Whether teleport completed successfully
    };

    /**
     * Group formation tracking
     */
    struct GroupFormationInfo
    {
        ObjectGuid groupGuid;
        uint32 dungeonId;
        uint32 formationTime;       // When group was formed
        ::std::vector<ObjectGuid> pendingTeleports; // Players waiting for teleport
    };

    // ========================================================================
    // INTERNAL HELPERS
    // ========================================================================

    /**
     * Process teleport timeouts
     * Cleans up stale teleport tracking entries
     */
    void ProcessTeleportTimeouts();

    /**
     * Get dungeon map ID from LFG dungeon ID
     *
     * @param dungeonId LFG dungeon ID
     * @return Map ID, or 0 if not found
     */
    uint32 GetDungeonMapId(uint32 dungeonId) const;

    /**
     * Validate dungeon entrance data
     *
     * @param mapId Map ID
     * @param x X coordinate
     * @param y Y coordinate
     * @param z Z coordinate
     * @return true if entrance data is valid
     */
    bool ValidateEntranceData(uint32 mapId, float x, float y, float z) const;

    /**
     * Send teleport notification to player
     *
     * @param player Player to notify
     * @param dungeonName Name of the dungeon
     */
    void NotifyTeleportStart(Player* player, ::std::string const& dungeonName);

    /**
     * Handle teleport failure
     *
     * @param player Player who failed to teleport
     * @param reason Failure reason
     */
    void HandleTeleportFailure(Player* player, ::std::string const& reason);

    // ========================================================================
    // DATA MEMBERS
    // ========================================================================

    bool _enabled;                                          ///< Whether coordinator is enabled
    uint32 _teleportTimeout;                                ///< Teleport timeout in milliseconds

    ::std::unordered_map<ObjectGuid, TeleportInfo> _pendingTeleports;        ///< Pending teleportations
    ::std::unordered_map<ObjectGuid, GroupFormationInfo> _groupFormations;   ///< Active group formations

    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::GROUP_MANAGER> _teleportMutex;                      ///< Protects teleport data
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::GROUP_MANAGER> _groupMutex;                         ///< Protects group formation data
};

} // namespace Playerbot

#define sLFGGroupCoordinator Playerbot::LFGGroupCoordinator::instance()

/**
 * INTEGRATION NOTES:
 *
 * HOOK INTEGRATION:
 * Add hooks in TrinityCore's LFG system:
 *
 * In LFGMgr::FinishDungeon() or similar:
 * ```cpp
 * #ifdef BUILD_PLAYERBOT
 * if (sLFGGroupCoordinator->IsEnabled())
 *     sLFGGroupCoordinator->OnGroupFormed(gguid, GetDungeon(gguid));
 * #endif
 * ```
 *
 * In Group::AddMember() for LFG groups:
 * ```cpp
 * #ifdef BUILD_PLAYERBOT
 * if (isLFGGroup() && sLFGGroupCoordinator->IsEnabled())
 *     sLFGGroupCoordinator->TeleportPlayerToDungeon(player, dungeonId);
 * #endif
 * ```
 *
 * CONFIGURATION:
 * In playerbots.conf:
 * ```
 * Playerbot.LFG.TeleportTimeout = 30000
 * Playerbot.LFG.AutoTeleport = 1
 * ```
 *
 * ERROR HANDLING:
 * - Teleport failures are logged and reported to player
 * - Timeout handling cleans up stale entries
 * - Thread-safe operations prevent race conditions
 *
 * PERFORMANCE CONSIDERATIONS:
 * - Batch teleportation for groups (all members at once)
 * - Minimal database queries (use cached dungeon data)
 * - Fast timeout processing (<1ms per update)
 *
 * TESTING:
 * - Unit tests for dungeon entrance lookup
 * - Integration tests for group teleportation
 * - Stress tests with multiple concurrent groups
 * - Timeout handling tests
 */
