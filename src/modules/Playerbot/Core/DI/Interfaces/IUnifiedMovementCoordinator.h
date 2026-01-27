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
#include <string>
#include <memory>

class Player;
class Unit;
class Group;
class Map;
struct Position;

namespace Playerbot
{

// Forward declarations from movement system
class MovementRequest;
struct MovementArbiterStatistics;
struct MovementArbiterConfig;
struct MovementPath;
struct FormationMember;
struct FormationCommand;
struct FormationConfig;
struct FormationMetrics;
struct MovementContext;
enum class MovementResult : uint8;  // Changed from struct - matches MovementTypes.h
struct PositionInfo;
struct PositionWeights;
struct AoEZone;
struct PositionMetrics;

// Full enum definitions needed for default parameters
enum class MovementFormationType : uint8
{
    NONE = 0,               // No formation
    LINE = 1,               // Single line formation
    COLUMN = 2,             // Single column formation
    WEDGE = 3,              // V-shaped wedge formation
    DIAMOND = 4,            // Diamond formation
    CIRCLE = 5,             // Circular formation
    BOX = 6,                // Rectangular box formation
    SPREAD = 7,             // Spread out formation
    STACK = 8,              // Tight stacked formation
    COMBAT_LINE = 9,        // Combat line with roles
    DUNGEON = 10,           // Dungeon formation (tank front, etc.)
    RAID = 11,              // Raid formation with groups
    ESCORT = 12,            // Escort formation around VIP
    FLANKING = 13,          // Flanking formation
    DEFENSIVE = 14          // Defensive circle formation
};

enum class FormationRole : uint8
{
    LEADER = 0,             // Formation leader (usually tank or group leader)
    TANK = 1,               // Tanking role in formation
    MELEE_DPS = 2,          // Melee damage dealers
    RANGED_DPS = 3,         // Ranged damage dealers
    HEALER = 4,             // Healers
    SUPPORT = 5,            // Support/utility members
    SCOUT = 6,              // Scouts/advance guard
    REAR_GUARD = 7,         // Rear guard protection
    FLANKER = 8,            // Flanking positions
    RESERVE = 9             // Reserve/flexible position
};

enum class PositionType : uint8
{
    MELEE_COMBAT = 0,       // Close combat positioning (2-5 yards)
    RANGED_DPS = 1,         // Ranged damage positioning (20-40 yards)
    HEALING = 2,            // Healing positioning (15-35 yards)
    KITING = 3,             // Kiting/mobility positioning (variable)
    FLANKING = 4,           // Flanking/behind target
    TANKING = 5,            // Tank positioning (front of enemy)
    SUPPORT = 6,            // Support/utility positioning
    RETREAT = 7,            // Retreat/escape positioning
    FORMATION = 8           // Group formation positioning
};

enum class FormationMovementState : uint8;
enum class FormationIntegrity : uint8;
enum class MovementPriority : uint8;
enum class PositionValidation : uint32;
enum class PlayerBotMovementPriority : uint8;  // Must match MovementPriorityMapper.h

/**
 * @brief Unified interface for all movement coordination operations
 *
 * This interface consolidates functionality from:
 * - MovementArbiter: Movement request arbitration
 * - PathfindingAdapter: Path calculation and caching
 * - FormationManager: Group formation management
 * - PositionManager: Combat positioning and tactical movement
 *
 * **Design Pattern:** Facade Pattern
 * - Single entry point for all movement operations
 * - Simplifies movement system interactions
 * - Reduces coupling between components
 *
 * **Benefits:**
 * - Easier to test (single mock instead of 4)
 * - Clear API surface
 * - Reduced complexity
 * - Better encapsulation
 */
class TC_GAME_API IUnifiedMovementCoordinator
{
public:
    virtual ~IUnifiedMovementCoordinator() = default;

    // ========================================================================
    // ARBITER MODULE (from MovementArbiter)
    // ========================================================================

    /**
     * @brief Movement request submission and management
     */
    virtual bool RequestMovement(MovementRequest const& request) = 0;
    virtual void ClearPendingRequests() = 0;
    virtual void StopMovement() = 0;
    virtual void Update(uint32 diff) = 0;

    /**
     * @brief Statistics and diagnostics
     */
    virtual MovementArbiterStatistics const& GetArbiterStatistics() const = 0;
    virtual void ResetArbiterStatistics() = 0;
    virtual ::std::string GetArbiterDiagnosticString() const = 0;
    virtual void LogArbiterStatistics() const = 0;

    /**
     * @brief Configuration
     */
    virtual MovementArbiterConfig GetArbiterConfig() const = 0;
    virtual void SetArbiterConfig(MovementArbiterConfig const& config) = 0;
    virtual void SetDiagnosticLogging(bool enable) = 0;

    /**
     * @brief Query current state
     */
    virtual uint32 GetPendingRequestCount() const = 0;
    virtual bool HasPendingRequests() const = 0;

    // ========================================================================
    // PATHFINDING MODULE (from PathfindingAdapter)
    // ========================================================================

    /**
     * @brief Initialization and shutdown
     */
    virtual bool InitializePathfinding(uint32 cacheSize = 100, uint32 cacheDuration = 5000) = 0;
    virtual void ShutdownPathfinding() = 0;

    /**
     * @brief Path calculation
     */
    virtual bool CalculatePath(Player* bot, Position const& destination, MovementPath& path, bool forceDirect = false) = 0;
    virtual bool CalculatePathToUnit(Player* bot, Unit* target, MovementPath& path, float range = 0.0f) = 0;
    virtual bool CalculateFormationPath(Player* bot, Unit* leader, Position const& offset, MovementPath& path) = 0;
    virtual bool CalculateFleePath(Player* bot, Unit* threat, float distance, MovementPath& path) = 0;

    /**
     * @brief Path caching
     */
    virtual bool HasCachedPath(Player* bot, Position const& destination) const = 0;
    virtual bool GetCachedPath(Player* bot, Position const& destination, MovementPath& path) const = 0;
    virtual void ClearPathCache(Player* bot) = 0;
    virtual void ClearAllPathCache() = 0;

    /**
     * @brief Path configuration
     */
    virtual void SetPathParameters(uint32 maxNodes = 3000, float straightDistance = 10.0f, float maxSearchDistance = 100.0f) = 0;
    virtual void EnablePathSmoothing(bool enable) = 0;
    virtual void EnablePathCaching(bool enable) = 0;
    virtual void SetCacheParameters(uint32 maxSize, uint32 duration) = 0;

    /**
     * @brief Path statistics
     */
    virtual void GetCacheStatistics(uint32& hits, uint32& misses, uint32& evictions) const = 0;
    virtual void GetPathStatistics(uint32& totalPaths, uint32& avgTime, uint32& maxTime) const = 0;
    virtual void ResetPathStatistics() = 0;

    /**
     * @brief Position validation
     */
    virtual bool IsWalkablePosition(Map* map, Position const& position) const = 0;
    virtual bool GetNearestWalkablePosition(Map* map, Position const& position, Position& walkable, float searchRange = 20.0f) const = 0;

    // ========================================================================
    // FORMATION MODULE (from FormationManager)
    // ========================================================================

    /**
     * @brief Formation management
     */
    virtual bool JoinFormation(::std::vector<Player*> const& groupMembers, MovementFormationType formation = MovementFormationType::DUNGEON) = 0;
    virtual bool LeaveFormation() = 0;
    virtual bool ChangeFormation(MovementFormationType newFormation) = 0;
    virtual bool SetFormationLeader(Player* leader) = 0;
    virtual Player* GetFormationLeader() const = 0;

    /**
     * @brief Formation execution
     */
    virtual void UpdateFormation(uint32 diff) = 0;
    virtual bool ExecuteFormationCommand(FormationCommand const& command) = 0;
    virtual bool MoveFormationToPosition(Position const& targetPos, float orientation = 0.0f) = 0;
    virtual bool AdjustFormationForCombat(::std::vector<Unit*> const& threats) = 0;

    /**
     * @brief Member management
     */
    virtual bool AddFormationMember(Player* player, FormationRole role = FormationRole::SUPPORT) = 0;
    virtual bool RemoveFormationMember(Player* player) = 0;
    virtual bool ChangeFormationMemberRole(Player* player, FormationRole newRole) = 0;
    virtual FormationMember* GetFormationMember(Player* player) = 0;
    virtual ::std::vector<FormationMember> GetAllFormationMembers() const = 0;

    /**
     * @brief Position calculation
     */
    virtual Position CalculateFormationPosition(FormationRole role, uint32 memberIndex) = 0;
    virtual ::std::vector<Position> CalculateAllFormationPositions() = 0;
    virtual Position GetAssignedFormationPosition() const = 0;
    virtual bool IsInFormationPosition(float tolerance = 2.0f) const = 0;

    /**
     * @brief Formation analysis
     */
    virtual FormationIntegrity AssessFormationIntegrity() = 0;
    virtual float CalculateCohesionLevel() = 0;
    virtual ::std::vector<Player*> GetOutOfPositionMembers(float tolerance = 3.0f) = 0;
    virtual bool RequiresReformation() = 0;

    /**
     * @brief Movement coordination
     */
    virtual void CoordinateFormationMovement(Position const& destination) = 0;
    virtual void MaintainFormationDuringMovement() = 0;
    virtual bool CanMoveWithoutBreakingFormation(Position const& newPos) = 0;
    virtual Position AdjustMovementForFormation(Position const& intendedPos) = 0;

    /**
     * @brief Combat formations
     */
    virtual void TransitionToCombatFormation(::std::vector<Unit*> const& enemies) = 0;
    virtual void TransitionToTravelFormation() = 0;
    virtual void AdjustForThreatSpread(::std::vector<Unit*> const& threats) = 0;
    virtual void HandleFormationBreakage() = 0;

    /**
     * @brief Role-specific formations
     */
    virtual MovementFormationType DetermineOptimalFormation(::std::vector<Player*> const& members) = 0;
    virtual FormationConfig GetFormationConfig(MovementFormationType formation) = 0;
    virtual void SetFormationConfig(MovementFormationType formation, FormationConfig const& config) = 0;

    /**
     * @brief Dynamic adjustments
     */
    virtual void AdjustFormationForTerrain() = 0;
    virtual void AdjustFormationForObstacles(::std::vector<Position> const& obstacles) = 0;
    virtual void AdjustFormationForGroupSize() = 0;
    virtual void HandleMemberDisconnection(Player* disconnectedMember) = 0;

    /**
     * @brief Query methods
     */
    virtual MovementFormationType GetCurrentFormation() const = 0;
    virtual FormationMovementState GetFormationMovementState() const = 0;
    virtual bool IsFormationLeader() const = 0;
    virtual bool IsInFormation() const = 0;
    virtual uint32 GetFormationMemberCount() const = 0;

    /**
     * @brief Configuration
     */
    virtual void SetFormationUpdateInterval(uint32 intervalMs) = 0;
    virtual uint32 GetFormationUpdateInterval() const = 0;
    virtual void SetCohesionRadius(float radius) = 0;
    virtual float GetCohesionRadius() const = 0;
    virtual void SetFormationSpacing(float spacing) = 0;
    virtual float GetFormationSpacing() const = 0;

    /**
     * @brief Performance monitoring
     */
    virtual FormationMetrics const& GetFormationMetrics() const = 0;
    virtual void ResetFormationMetrics() = 0;

    /**
     * @brief Advanced features
     */
    virtual void EnableAdaptiveFormations(bool enable) = 0;
    virtual bool IsAdaptiveFormationsEnabled() const = 0;
    virtual void SetFormationPriority(uint32 priority) = 0;
    virtual uint32 GetFormationPriority() const = 0;

    /**
     * @brief Emergency handling
     */
    virtual void ActivateEmergencyScatter() = 0;
    virtual void DeactivateEmergencyScatter() = 0;
    virtual bool IsEmergencyScatterActive() const = 0;
    virtual void HandleEmergencyRegroup(Position const& rallyPoint) = 0;

    // ========================================================================
    // POSITION MODULE (from PositionManager)
    // ========================================================================

    /**
     * @brief Main positioning interface
     */
    virtual MovementResult UpdatePosition(MovementContext const& context) = 0;
    virtual MovementResult FindOptimalPosition(MovementContext const& context) = 0;
    virtual MovementResult ExecuteMovement(Position const& targetPos, MovementPriority priority) = 0;

    /**
     * @brief Position evaluation
     */
    virtual PositionInfo EvaluatePosition(Position const& pos, MovementContext const& context) = 0;
    virtual ::std::vector<PositionInfo> EvaluatePositions(::std::vector<Position> const& positions, MovementContext const& context) = 0;
    virtual ::std::vector<Position> GenerateCandidatePositions(MovementContext const& context) = 0;

    /**
     * @brief Range and angle management
     */
    virtual Position FindRangePosition(Unit* target, float minRange, float maxRange, float preferredAngle = 0.0f) = 0;
    virtual Position FindMeleePosition(Unit* target, bool preferBehind = true) = 0;
    virtual Position FindRangedPosition(Unit* target, float preferredRange = 25.0f) = 0;
    virtual Position FindHealingPosition(::std::vector<Player*> const& allies) = 0;
    virtual Position FindKitingPosition(Unit* threat, float minDistance = 15.0f) = 0;

    /**
     * @brief Role-specific positioning
     */
    virtual Position FindTankPosition(Unit* target) = 0;
    virtual Position FindDpsPosition(Unit* target, PositionType type = PositionType::MELEE_COMBAT) = 0;
    virtual Position FindHealerPosition(::std::vector<Player*> const& groupMembers) = 0;
    virtual Position FindSupportPosition(::std::vector<Player*> const& groupMembers) = 0;

    /**
     * @brief Safety and avoidance
     */
    virtual bool IsPositionSafe(Position const& pos, MovementContext const& context) = 0;
    virtual bool IsInDangerZone(Position const& pos) = 0;
    virtual Position FindSafePosition(Position const& fromPos, float minDistance = 10.0f) = 0;
    virtual Position FindEscapePosition(::std::vector<Unit*> const& threats) = 0;

    /**
     * @brief AoE and hazard management
     */
    virtual void RegisterAoEZone(AoEZone const& zone) = 0;
    virtual void UpdateAoEZones(uint32 currentTime) = 0;
    virtual void ClearExpiredZones(uint32 currentTime) = 0;
    virtual ::std::vector<AoEZone> GetActiveZones() const = 0;

    /**
     * @brief Validation and pathfinding
     */
    virtual bool ValidatePosition(Position const& pos, PositionValidation flags) = 0;
    virtual bool HasLineOfSight(Position const& from, Position const& to) = 0;
    virtual bool IsWalkable(Position const& pos) = 0;
    virtual float CalculateMovementCost(Position const& from, Position const& to) = 0;

    /**
     * @brief Group coordination
     */
    virtual Position FindFormationPositionForRole(::std::vector<Player*> const& groupMembers, PositionType formationType) = 0;
    virtual bool ShouldMaintainGroupProximity() = 0;
    virtual float GetOptimalGroupDistance(uint8 role) = 0;

    /**
     * @brief Advanced movement features
     */
    virtual bool ShouldStrafe(Unit* target) = 0;
    virtual bool ShouldCircleStrafe(Unit* target) = 0;
    virtual Position CalculateStrafePosition(Unit* target, bool strafeLeft = true) = 0;
    virtual Position PredictTargetPosition(Unit* target, float timeAhead) = 0;

    /**
     * @brief Configuration
     */
    virtual void SetPositionWeights(PositionWeights const& weights) = 0;
    virtual PositionWeights const& GetPositionWeights() const = 0;
    virtual void SetPositionUpdateInterval(uint32 intervalMs) = 0;
    virtual uint32 GetPositionUpdateInterval() const = 0;

    /**
     * @brief Performance monitoring
     */
    virtual PositionMetrics const& GetPositionMetrics() const = 0;
    virtual void ResetPositionMetrics() = 0;

    /**
     * @brief Emergency response
     */
    virtual MovementResult HandleEmergencyMovement(MovementContext const& context) = 0;
    virtual bool IsInEmergencyPosition() = 0;
    virtual Position FindEmergencyEscapePosition() = 0;

    /**
     * @brief Position history and learning
     */
    virtual void RecordPositionSuccess(Position const& pos, PositionType type) = 0;
    virtual void RecordPositionFailure(Position const& pos, ::std::string const& reason) = 0;
    virtual float GetPositionSuccessRate(Position const& pos, float radius = 5.0f) = 0;

    // ========================================================================
    // UNIFIED OPERATIONS (combining all modules)
    // ========================================================================

    /**
     * @brief Complete movement coordination
     *
     * This method orchestrates the entire movement flow:
     * 1. Position evaluation (Position module)
     * 2. Path calculation (Pathfinding module)
     * 3. Formation adjustment (Formation module)
     * 4. Movement request arbitration (Arbiter module)
     *
     * @param bot Bot to coordinate movement for
     * @param context Movement context
     */
    virtual void CoordinateCompleteMovement(Player* bot, MovementContext const& context) = 0;

    /**
     * @brief Get comprehensive movement recommendation
     *
     * Combines position evaluation, pathfinding, and formation analysis to provide:
     * - Optimal position
     * - Path quality
     * - Formation impact
     * - Movement priority
     * - Reasoning
     *
     * @param bot Bot context
     * @param context Movement context
     * @return Detailed recommendation string
     */
    virtual ::std::string GetMovementRecommendation(Player* bot, MovementContext const& context) = 0;

    /**
     * @brief Optimize movement for a bot
     *
     * Analyzes current movement state and suggests:
     * - Optimal positioning
     * - Formation adjustments
     * - Path optimizations
     * - Movement request cleanup
     *
     * @param bot Bot to optimize for
     */
    virtual void OptimizeBotMovement(Player* bot) = 0;

    /**
     * @brief Get statistics for movement operations
     * @return Statistics string (for debugging/monitoring)
     */
    virtual ::std::string GetMovementStatistics() const = 0;
};

} // namespace Playerbot
