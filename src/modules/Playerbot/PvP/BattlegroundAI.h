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
#include "Player.h"
#include "ObjectGuid.h"
#include "Position.h"
#include "Battleground.h"
#include "../AI/Coordination/Battleground/BGState.h"
#include "Core/DI/Interfaces/IBattlegroundAI.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace Playerbot
{

// Forward declarations
class BattlegroundCoordinator;

namespace Coordination::Battleground {
    class IBGScript;
}

// All BG types (BGType, BGRole, BGObjective, etc.) are defined in BGState.h

/**
 * @brief BG strategy profile
 */
struct BGStrategyProfile
{
    bool autoAssignRoles = true;
    bool autoDefendBases = true;
    bool autoCaptureBases = true;
    bool prioritizeHealers = true;    // Protect healers
    bool groupUpForObjectives = true; // Wait for group before attacking
    uint32 minPlayersForAttack = 3;   // Min players to attack objective
    float defenseRadius = 30.0f;      // Radius to defend objectives
    bool allowRoleSwitch = true;      // Can switch roles mid-game

    BGStrategyProfile() = default;
};

/**
 * @brief Warsong Gulch / Twin Peaks strategy
 */
struct FlagBGStrategy
{
    bool escortFlagCarrier = true;
    bool defendFlagRoom = true;
    bool killEnemyFC = true;          // Priority: kill enemy flag carrier
    uint32 fcEscortCount = 3;         // Players escorting FC
    uint32 flagRoomDefenders = 2;     // Players defending flag room
    Position friendlyFlagSpawn;
    Position enemyFlagSpawn;

    FlagBGStrategy() : fcEscortCount(3), flagRoomDefenders(2) {}
};

/**
 * @brief Arathi Basin / Battle for Gilneas strategy
 */
struct BaseBGStrategy
{
    ::std::vector<Position> baseLocations;
    ::std::unordered_map<uint32, uint32> baseDefenderCount; // baseId -> defender count
    ::std::vector<uint32> priorityBases;  // Bases to prioritize (e.g., Blacksmith in AB)
    bool rotateCaptures = true;         // Rotate between bases
    uint32 minDefendersPerBase = 2;

    BaseBGStrategy() : rotateCaptures(true), minDefendersPerBase(2) {}
};

/**
 * @brief Alterac Valley strategy
 */
struct AVStrategy
{
    bool captureGraveyards = true;
    bool captureTowers = true;
    bool killBoss = true;
    bool escortNPCs = true;
    bool collectResources = true;
    ::std::vector<Position> graveyardLocations;
    ::std::vector<Position> towerLocations;
    Position bossLocation;

    AVStrategy() : captureGraveyards(true), captureTowers(true),
        killBoss(true), escortNPCs(true), collectResources(true) {}
};

/**
 * @brief Eye of the Storm strategy
 */
struct EOTSStrategy
{
    bool captureBases = true;
    bool captureFlag = true;
    bool prioritizeFlagWhenLeading = false; // Focus flag if winning
    ::std::vector<Position> baseLocations;
    Position flagLocation;
    uint32 flagCarrierEscortCount = 3;

    EOTSStrategy() : captureBases(true), captureFlag(true),
        prioritizeFlagWhenLeading(false), flagCarrierEscortCount(3) {}
};

/**
 * @brief Strand of the Ancients / Isle of Conquest strategy
 */
struct SiegeStrategy
{
    bool operateSiegeWeapons = true;
    bool defendGates = true;
    bool attackGates = true;
    bool prioritizeDemolishers = true;
    ::std::vector<Position> gateLocations;
    ::std::vector<Position> siegeWeaponLocations;

    SiegeStrategy() : operateSiegeWeapons(true), defendGates(true),
        attackGates(true), prioritizeDemolishers(true) {}
};

/**
 * @brief BG performance metrics
 */
struct BGMetrics
{
    ::std::atomic<uint32> objectivesCaptured{0};
    ::std::atomic<uint32> objectivesDefended{0};
    ::std::atomic<uint32> flagCaptures{0};
    ::std::atomic<uint32> flagReturns{0};
    ::std::atomic<uint32> basesAssaulted{0};
    ::std::atomic<uint32> basesDefended{0};
    ::std::atomic<uint32> matchesWon{0};
    ::std::atomic<uint32> matchesLost{0};

    void Reset()
    {
        objectivesCaptured = 0;
        objectivesDefended = 0;
        flagCaptures = 0;
        flagReturns = 0;
        basesAssaulted = 0;
        basesDefended = 0;
        matchesWon = 0;
        matchesLost = 0;
    }

    float GetWinRate() const
    {
        uint32 total = matchesWon.load() + matchesLost.load();
        return total > 0 ? static_cast<float>(matchesWon.load()) / total : 0.0f;
    }
};

/**
 * @brief Battleground AI - Complete BG automation
 *
 * Features:
 * - Automatic role assignment
 * - Objective-based strategies
 * - BG-specific tactics (WSG, AB, AV, EOTS, etc.)
 * - Team coordination
 * - Resource management
 * - Adaptive strategies based on score
 */
class TC_GAME_API BattlegroundAI final : public IBattlegroundAI
{
public:
    static BattlegroundAI* instance();

    // ============================================================================
    // INITIALIZATION
    // ============================================================================

    void Initialize() override;
    void Update(::Player* player, uint32 diff) override;

    // ============================================================================
    // ROLE MANAGEMENT
    // ============================================================================

    /**
     * Assign role to player based on class/spec
     */
    void AssignRole(::Player* player, BGType bgType) override;

    /**
     * Get player's current BG role
     */
    BGRole GetPlayerRole(::Player* player) const override;

    /**
     * Switch player to new role
     */
    bool SwitchRole(::Player* player, BGRole newRole) override;

    /**
     * Check if role is appropriate for class
     */
    bool IsRoleAppropriate(::Player* player, BGRole role) const override;

    // ============================================================================
    // OBJECTIVE MANAGEMENT
    // ============================================================================

    /**
     * Get all active objectives for battleground
     */
    ::std::vector<BGObjective> GetActiveObjectives(::Player* player) const override;

    /**
     * Get highest priority objective for player
     */
    BGObjective GetPlayerObjective(::Player* player) const override;

    /**
     * Assign players to objective
     */
    bool AssignObjective(::Player* player, BGObjective const& objective) override;

    /**
     * Complete objective
     */
    bool CompleteObjective(::Player* player, BGObjective const& objective) override;

    /**
     * Check if objective is being attacked
     */
    bool IsObjectiveContested(BGObjective const& objective) const override;

    // ============================================================================
    // BG-SPECIFIC STRATEGIES
    // ============================================================================

    // Warsong Gulch / Twin Peaks
    void ExecuteWSGStrategy(::Player* player) override;
    bool PickupFlag(::Player* player);
    bool ReturnFlag(::Player* player);
    ::Player* FindFriendlyFlagCarrier(::Player* player) const;
    ::Player* FindEnemyFlagCarrier(::Player* player) const;
    bool EscortFlagCarrier(::Player* player, ::Player* fc);
    bool DefendFlagRoom(::Player* player);

    // CTF Behavior Execution (script-integrated)
    void ExecuteFlagCarrierBehavior(::Player* player,
        BattlegroundCoordinator* coordinator,
        Coordination::Battleground::IBGScript* script);
    void ExecuteFlagPickupBehavior(::Player* player,
        BattlegroundCoordinator* coordinator,
        Coordination::Battleground::IBGScript* script);
    void ExecuteFlagHunterBehavior(::Player* player, ::Player* enemyFC);
    void ExecuteEscortBehavior(::Player* player, ::Player* friendlyFC,
        BattlegroundCoordinator* coordinator,
        Coordination::Battleground::IBGScript* script);
    void ExecuteDefenderBehavior(::Player* player,
        BattlegroundCoordinator* coordinator,
        Coordination::Battleground::IBGScript* script);

    // Arathi Basin / Battle for Gilneas
    void ExecuteABStrategy(::Player* player) override;
    bool CaptureBase(::Player* player, Position const& baseLocation);
    bool DefendBase(::Player* player, Position const& baseLocation);
    Position FindBestBaseToCapture(::Player* player) const;
    ::std::vector<Position> GetCapturedBases(::Player* player) const;
    bool IsBaseUnderAttack(Position const& baseLocation) const;

    // Alterac Valley
    void ExecuteAVStrategy(::Player* player) override;
    bool CaptureGraveyard(::Player* player);
    bool CaptureTower(::Player* player);
    bool KillBoss(::Player* player);
    bool EscortNPC(::Player* player);

    // Eye of the Storm
    void ExecuteEOTSStrategy(::Player* player) override;
    bool CaptureFlagEOTS(::Player* player);
    bool CaptureBaseEOTS(::Player* player);

    // Strand of the Ancients / Isle of Conquest
    void ExecuteSiegeStrategy(::Player* player) override;
    bool OperateSiegeWeapon(::Player* player);
    bool AttackGate(::Player* player);
    bool DefendGate(::Player* player);

    // Temple of Kotmogu
    void ExecuteKotmoguStrategy(::Player* player) override;
    bool PickupOrb(::Player* player);
    bool DefendOrbCarrier(::Player* player);

    // Silvershard Mines
    void ExecuteSilvershardStrategy(::Player* player) override;
    bool CaptureCart(::Player* player);
    bool DefendCart(::Player* player);

    // Deepwind Gorge
    void ExecuteDeepwindStrategy(::Player* player) override;
    bool CaptureMine(::Player* player);
    bool DefendMine(::Player* player);

    // ============================================================================
    // TEAM COORDINATION
    // ============================================================================

    /**
     * Group up for objective
     */
    bool GroupUpForObjective(::Player* player, BGObjective const& objective) override;

    /**
     * Find nearby team members
     */
    ::std::vector<::Player*> GetNearbyTeammates(::Player* player, float range) const override;

    /**
     * Call for backup at location
     */
    bool CallForBackup(::Player* player, Position const& location) override;

    /**
     * Respond to backup call
     */
    bool RespondToBackupCall(::Player* player, Position const& location) override;

    // ============================================================================
    // POSITIONING
    // ============================================================================

    /**
     * Move to objective location
     */
    bool MoveToObjective(::Player* player, BGObjective const& objective) override;

    /**
     * Take defensive position
     */
    bool TakeDefensivePosition(::Player* player, Position const& location) override;

    /**
     * Check if player is at objective
     */
    bool IsAtObjective(::Player* player, BGObjective const& objective) const override;

    // ============================================================================
    // ADAPTIVE STRATEGY
    // ============================================================================

    /**
     * Adjust strategy based on score
     */
    void AdjustStrategyBasedOnScore(::Player* player) override;

    /**
     * Check if team is winning
     */
    bool IsTeamWinning(::Player* player) const override;

    /**
     * Switch to defensive strategy when winning
     */
    void SwitchToDefensiveStrategy(::Player* player) override;

    /**
     * Switch to aggressive strategy when losing
     */
    void SwitchToAggressiveStrategy(::Player* player) override;

    // ============================================================================
    // PROFILES
    // ============================================================================

    void SetStrategyProfile(uint32 playerGuid, BGStrategyProfile const& profile) override;
    BGStrategyProfile GetStrategyProfile(uint32 playerGuid) const override;

    // ============================================================================
    // METRICS
    // ============================================================================

    BGMetrics const& GetPlayerMetrics(uint32 playerGuid) const override;
    BGMetrics const& GetGlobalMetrics() const override;

private:
    BattlegroundAI();
    ~BattlegroundAI() = default;

    // ============================================================================
    // HELPER FUNCTIONS
    // ============================================================================

    BGType GetBattlegroundType(::Player* player) const;
    ::Battleground* GetPlayerBattleground(::Player* player) const;
    uint32 GetTeamScore(::Player* player) const;
    uint32 GetEnemyTeamScore(::Player* player) const;
    bool IsObjectiveInRange(::Player* player, Position const& objLocation, float range) const;
    uint32 CountPlayersAtObjective(Position const& objLocation, float range) const;
    ::std::vector<::Player*> GetPlayersAtObjective(Position const& objLocation, float range) const;

    // ============================================================================
    // INITIALIZATION HELPERS
    // ============================================================================

    void InitializeWSGStrategy();
    void InitializeABStrategy();
    void InitializeAVStrategy();
    void InitializeEOTSStrategy();
    void InitializeSiegeStrategy();

    // ============================================================================
    // DATA STRUCTURES
    // ============================================================================

    // Strategy profiles
    ::std::unordered_map<uint32, BGStrategyProfile> _playerProfiles;

    // Role assignments (playerGuid -> role)
    ::std::unordered_map<uint32, BGRole> _playerRoles;

    // Objective assignments (playerGuid -> objective)
    ::std::unordered_map<uint32, BGObjective> _playerObjectives;

    // Active objectives per BG (bgInstanceId -> objectives)
    ::std::unordered_map<uint32, ::std::vector<BGObjective>> _activeObjectives;

    // BG-specific strategies
    ::std::unordered_map<BGType, FlagBGStrategy> _flagStrategies;
    ::std::unordered_map<BGType, BaseBGStrategy> _baseStrategies;
    ::std::unordered_map<BGType, AVStrategy> _avStrategies;
    ::std::unordered_map<BGType, EOTSStrategy> _eotsStrategies;
    ::std::unordered_map<BGType, SiegeStrategy> _siegeStrategies;

    // Backup calls (location -> timestamp)
    ::std::unordered_map<uint32, ::std::pair<Position, uint32>> _backupCalls;

    // Metrics
    ::std::unordered_map<uint32, BGMetrics> _playerMetrics;
    BGMetrics _globalMetrics;

    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BEHAVIOR_MANAGER> _mutex;

    // Update intervals
    static constexpr uint32 BG_UPDATE_INTERVAL = 500;  // 500ms
    ::std::unordered_map<uint32, uint32> _lastUpdateTimes;

    // Constants
    static constexpr float OBJECTIVE_RANGE = 10.0f;
    static constexpr float BACKUP_CALL_RANGE = 50.0f;
    static constexpr uint32 BACKUP_CALL_DURATION = 30000; // 30 seconds
};

} // namespace Playerbot
