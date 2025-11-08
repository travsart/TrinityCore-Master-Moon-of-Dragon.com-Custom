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
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace Playerbot
{

/**
 * @brief Battleground types
 */
enum class BGType : uint8
{
    WARSONG_GULCH = 0,
    ARATHI_BASIN = 1,
    ALTERAC_VALLEY = 2,
    EYE_OF_THE_STORM = 3,
    STRAND_OF_THE_ANCIENTS = 4,
    ISLE_OF_CONQUEST = 5,
    TWIN_PEAKS = 6,
    BATTLE_FOR_GILNEAS = 7,
    TEMPLE_OF_KOTMOGU = 8,
    SILVERSHARD_MINES = 9,
    DEEPWIND_GORGE = 10
};

/**
 * @brief Battleground role assignment
 */
enum class BGRole : uint8
{
    FLAG_CARRIER = 0,      // WSG/TP - Carry flag
    FLAG_DEFENDER = 1,     // WSG/TP - Defend flag room
    ATTACKER = 2,          // Generic offensive role
    DEFENDER = 3,          // Generic defensive role
    BASE_CAPTURER = 4,     // AB/EOTS - Capture bases
    BASE_DEFENDER = 5,     // AB/EOTS - Defend bases
    SIEGE_OPERATOR = 6,    // AV/IoC/SotA - Operate siege weapons
    HEALER_SUPPORT = 7,    // Support role for healers
    SCOUT = 8              // Scouting and intelligence
};

/**
 * @brief Objective priority
 */
enum class ObjectivePriority : uint8
{
    CRITICAL = 0,          // Must complete immediately
    HIGH = 1,              // Important but not urgent
    MEDIUM = 2,            // Standard priority
    LOW = 3,               // Optional objective
    NONE = 4               // No priority
};

/**
 * @brief BG objective types
 */
enum class BGObjectiveType : uint8
{
    CAPTURE_FLAG = 0,
    DEFEND_FLAG = 1,
    CAPTURE_BASE = 2,
    DEFEND_BASE = 3,
    KILL_BOSS = 4,
    ESCORT_NPC = 5,
    OPERATE_SIEGE = 6,
    COLLECT_ORB = 7,
    CAPTURE_CART = 8
};

/**
 * @brief Battleground objective
 */
struct BGObjective
{
    BGObjectiveType type;
    Position location;
    ObjectGuid objectGuid;  // Flag, base, NPC, etc.
    ObjectivePriority priority;
    uint32 playersRequired;
    uint32 playersAssigned;
    bool isCompleted;
    uint32 timeRemaining;   // Seconds until objective expires

    BGObjective() : type(BGObjectiveType::CAPTURE_FLAG), priority(ObjectivePriority::NONE),
        playersRequired(1), playersAssigned(0), isCompleted(false), timeRemaining(0) {}
};

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
    std::vector<Position> baseLocations;
    std::unordered_map<uint32, uint32> baseDefenderCount; // baseId -> defender count
    std::vector<uint32> priorityBases;  // Bases to prioritize (e.g., Blacksmith in AB)
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
    std::vector<Position> graveyardLocations;
    std::vector<Position> towerLocations;
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
    std::vector<Position> baseLocations;
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
    std::vector<Position> gateLocations;
    std::vector<Position> siegeWeaponLocations;

    SiegeStrategy() : operateSiegeWeapons(true), defendGates(true),
        attackGates(true), prioritizeDemolishers(true) {}
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
class TC_GAME_API BattlegroundAI
{
public:
    static BattlegroundAI* instance();

    // ============================================================================
    // INITIALIZATION
    // ============================================================================

    void Initialize();
    void Update(::Player* player, uint32 diff);

    // ============================================================================
    // ROLE MANAGEMENT
    // ============================================================================

    /**
     * Assign role to player based on class/spec
     */
    void AssignRole(::Player* player, BGType bgType);

    /**
     * Get player's current BG role
     */
    BGRole GetPlayerRole(::Player* player) const;

    /**
     * Switch player to new role
     */
    bool SwitchRole(::Player* player, BGRole newRole);

    /**
     * Check if role is appropriate for class
     */
    bool IsRoleAppropriate(::Player* player, BGRole role) const;

    // ============================================================================
    // OBJECTIVE MANAGEMENT
    // ============================================================================

    /**
     * Get all active objectives for battleground
     */
    std::vector<BGObjective> GetActiveObjectives(::Player* player) const;

    /**
     * Get highest priority objective for player
     */
    BGObjective GetPlayerObjective(::Player* player) const;

    /**
     * Assign players to objective
     */
    bool AssignObjective(::Player* player, BGObjective const& objective);

    /**
     * Complete objective
     */
    bool CompleteObjective(::Player* player, BGObjective const& objective);

    /**
     * Check if objective is being attacked
     */
    bool IsObjectiveContested(BGObjective const& objective) const;

    // ============================================================================
    // BG-SPECIFIC STRATEGIES
    // ============================================================================

    // Warsong Gulch / Twin Peaks
    void ExecuteWSGStrategy(::Player* player);
    bool PickupFlag(::Player* player);
    bool ReturnFlag(::Player* player);
    ::Player* FindFriendlyFlagCarrier(::Player* player) const;
    ::Player* FindEnemyFlagCarrier(::Player* player) const;
    bool EscortFlagCarrier(::Player* player, ::Player* fc);
    bool DefendFlagRoom(::Player* player);

    // Arathi Basin / Battle for Gilneas
    void ExecuteABStrategy(::Player* player);
    bool CaptureBase(::Player* player, Position const& baseLocation);
    bool DefendBase(::Player* player, Position const& baseLocation);
    Position FindBestBaseToCapture(::Player* player) const;
    std::vector<Position> GetCapturedBases(::Player* player) const;
    bool IsBaseUnderAttack(Position const& baseLocation) const;

    // Alterac Valley
    void ExecuteAVStrategy(::Player* player);
    bool CaptureGraveyard(::Player* player);
    bool CaptureTower(::Player* player);
    bool KillBoss(::Player* player);
    bool EscortNPC(::Player* player);

    // Eye of the Storm
    void ExecuteEOTSStrategy(::Player* player);
    bool CaptureFlagEOTS(::Player* player);
    bool CaptureBaseEOTS(::Player* player);

    // Strand of the Ancients / Isle of Conquest
    void ExecuteSiegeStrategy(::Player* player);
    bool OperateSiegeWeapon(::Player* player);
    bool AttackGate(::Player* player);
    bool DefendGate(::Player* player);

    // Temple of Kotmogu
    void ExecuteKotmoguStrategy(::Player* player);
    bool PickupOrb(::Player* player);
    bool DefendOrbCarrier(::Player* player);

    // Silvershard Mines
    void ExecuteSilvershardStrategy(::Player* player);
    bool CaptureCart(::Player* player);
    bool DefendCart(::Player* player);

    // Deepwind Gorge
    void ExecuteDeepwindStrategy(::Player* player);
    bool CaptureMine(::Player* player);
    bool DefendMine(::Player* player);

    // ============================================================================
    // TEAM COORDINATION
    // ============================================================================

    /**
     * Group up for objective
     */
    bool GroupUpForObjective(::Player* player, BGObjective const& objective);

    /**
     * Find nearby team members
     */
    std::vector<::Player*> GetNearbyTeammates(::Player* player, float range) const;

    /**
     * Call for backup at location
     */
    bool CallForBackup(::Player* player, Position const& location);

    /**
     * Respond to backup call
     */
    bool RespondToBackupCall(::Player* player, Position const& location);

    // ============================================================================
    // POSITIONING
    // ============================================================================

    /**
     * Move to objective location
     */
    bool MoveToObjective(::Player* player, BGObjective const& objective);

    /**
     * Take defensive position
     */
    bool TakeDefensivePosition(::Player* player, Position const& location);

    /**
     * Check if player is at objective
     */
    bool IsAtObjective(::Player* player, BGObjective const& objective) const;

    // ============================================================================
    // ADAPTIVE STRATEGY
    // ============================================================================

    /**
     * Adjust strategy based on score
     */
    void AdjustStrategyBasedOnScore(::Player* player);

    /**
     * Check if team is winning
     */
    bool IsTeamWinning(::Player* player) const;

    /**
     * Switch to defensive strategy when winning
     */
    void SwitchToDefensiveStrategy(::Player* player);

    /**
     * Switch to aggressive strategy when losing
     */
    void SwitchToAggressiveStrategy(::Player* player);

    // ============================================================================
    // PROFILES
    // ============================================================================

    void SetStrategyProfile(uint32 playerGuid, BGStrategyProfile const& profile);
    BGStrategyProfile GetStrategyProfile(uint32 playerGuid) const;

    // ============================================================================
    // METRICS
    // ============================================================================

    struct BGMetrics
    {
        std::atomic<uint32> objectivesCaptured{0};
        std::atomic<uint32> objectivesDefended{0};
        std::atomic<uint32> flagCaptures{0};
        std::atomic<uint32> flagReturns{0};
        std::atomic<uint32> basesAssaulted{0};
        std::atomic<uint32> basesDefended{0};
        std::atomic<uint32> matchesWon{0};
        std::atomic<uint32> matchesLost{0};

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

    BGMetrics const& GetPlayerMetrics(uint32 playerGuid) const;
    BGMetrics const& GetGlobalMetrics() const;

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
    std::vector<::Player*> GetPlayersAtObjective(Position const& objLocation, float range) const;

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
    std::unordered_map<uint32, BGStrategyProfile> _playerProfiles;

    // Role assignments (playerGuid -> role)
    std::unordered_map<uint32, BGRole> _playerRoles;

    // Objective assignments (playerGuid -> objective)
    std::unordered_map<uint32, BGObjective> _playerObjectives;

    // Active objectives per BG (bgInstanceId -> objectives)
    std::unordered_map<uint32, std::vector<BGObjective>> _activeObjectives;

    // BG-specific strategies
    std::unordered_map<BGType, FlagBGStrategy> _flagStrategies;
    std::unordered_map<BGType, BaseBGStrategy> _baseStrategies;
    std::unordered_map<BGType, AVStrategy> _avStrategies;
    std::unordered_map<BGType, EOTSStrategy> _eotsStrategies;
    std::unordered_map<BGType, SiegeStrategy> _siegeStrategies;

    // Backup calls (location -> timestamp)
    std::unordered_map<uint32, std::pair<Position, uint32>> _backupCalls;

    // Metrics
    std::unordered_map<uint32, BGMetrics> _playerMetrics;
    BGMetrics _globalMetrics;

    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BEHAVIOR_MANAGER> _mutex;

    // Update intervals
    static constexpr uint32 BG_UPDATE_INTERVAL = 500;  // 500ms
    std::unordered_map<uint32, uint32> _lastUpdateTimes;

    // Constants
    static constexpr float OBJECTIVE_RANGE = 10.0f;
    static constexpr float BACKUP_CALL_RANGE = 50.0f;
    static constexpr uint32 BACKUP_CALL_DURATION = 30000; // 30 seconds
};

} // namespace Playerbot
