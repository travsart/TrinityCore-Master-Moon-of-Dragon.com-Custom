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
#include "Core/DI/Interfaces/IUnifiedMovementCoordinator.h"
#include "Arbiter/MovementArbiter.h"
#include "Pathfinding/PathfindingAdapter.h"
#include "AI/Combat/FormationManager.h"
#include "AI/Combat/PositionManager.h"
#include "Player.h"
#include "Group.h"
#include "Position.h"
#include <memory>
#include <atomic>

namespace Playerbot
{

/**
 * @brief Unified movement coordination system
 *
 * Consolidates four separate managers into one cohesive system:
 * - MovementArbiter: Movement request arbitration
 * - PathfindingAdapter: Path calculation and caching
 * - FormationManager: Group formation management
 * - PositionManager: Combat positioning and tactical movement
 *
 * **Architecture:**
 * ```
 * UnifiedMovementCoordinator
 *   > ArbiterModule       (movement request arbitration)
 *   > PathfindingModule   (path calculation, caching)
 *   > FormationModule     (group formations)
 *   > PositionModule      (combat positioning)
 * ```
 *
 * **Thread Safety:**
 * - Uses OrderedMutex<MOVEMENT_ARBITER> for all operations
 * - Modules share data through thread-safe interfaces
 * - Lock ordering prevents deadlocks
 *
 * **Migration Path:**
 * - Old managers (MovementArbiter, PathfindingAdapter, etc.) still work
 * - New code should use UnifiedMovementCoordinator
 * - Gradually migrate callsites over time
 * - Eventually deprecate old managers
 */
class TC_GAME_API UnifiedMovementCoordinator final : public IUnifiedMovementCoordinator
{
public:
    explicit UnifiedMovementCoordinator(Player* bot);
    ~UnifiedMovementCoordinator() override;

    // Non-copyable, non-movable
    UnifiedMovementCoordinator(UnifiedMovementCoordinator const&) = delete;
    UnifiedMovementCoordinator& operator=(UnifiedMovementCoordinator const&) = delete;
    UnifiedMovementCoordinator(UnifiedMovementCoordinator&&) = delete;
    UnifiedMovementCoordinator& operator=(UnifiedMovementCoordinator&&) = delete;

    // ========================================================================
    // ARBITER MODULE INTERFACE
    // ========================================================================

    bool RequestMovement(MovementRequest const& request) override;
    void ClearPendingRequests() override;
    void StopMovement() override;
    void Update(uint32 diff) override;
    MovementArbiterStatistics const& GetArbiterStatistics() const override;
    void ResetArbiterStatistics() override;
    ::std::string GetArbiterDiagnosticString() const override;
    void LogArbiterStatistics() const override;
    MovementArbiterConfig GetArbiterConfig() const override;
    void SetArbiterConfig(MovementArbiterConfig const& config) override;
    void SetDiagnosticLogging(bool enable) override;
    uint32 GetPendingRequestCount() const override;
    bool HasPendingRequests() const override;

    // ========================================================================
    // PATHFINDING MODULE INTERFACE
    // ========================================================================

    bool InitializePathfinding(uint32 cacheSize = 100, uint32 cacheDuration = 5000) override;
    void ShutdownPathfinding() override;
    bool CalculatePath(Player* bot, Position const& destination, MovementPath& path, bool forceDirect = false) override;
    bool CalculatePathToUnit(Player* bot, Unit* target, MovementPath& path, float range = 0.0f) override;
    bool CalculateFormationPath(Player* bot, Unit* leader, Position const& offset, MovementPath& path) override;
    bool CalculateFleePath(Player* bot, Unit* threat, float distance, MovementPath& path) override;
    bool HasCachedPath(Player* bot, Position const& destination) const override;
    bool GetCachedPath(Player* bot, Position const& destination, MovementPath& path) const override;
    void ClearPathCache(Player* bot) override;
    void ClearAllPathCache() override;
    void SetPathParameters(uint32 maxNodes = 3000, float straightDistance = 10.0f, float maxSearchDistance = 100.0f) override;
    void EnablePathSmoothing(bool enable) override;
    void EnablePathCaching(bool enable) override;
    void SetCacheParameters(uint32 maxSize, uint32 duration) override;
    void GetCacheStatistics(uint32& hits, uint32& misses, uint32& evictions) const override;
    void GetPathStatistics(uint32& totalPaths, uint32& avgTime, uint32& maxTime) const override;
    void ResetPathStatistics() override;
    bool IsWalkablePosition(Map* map, Position const& position) const override;
    bool GetNearestWalkablePosition(Map* map, Position const& position, Position& walkable, float searchRange = 20.0f) const override;

    // ========================================================================
    // FORMATION MODULE INTERFACE
    // ========================================================================

    bool JoinFormation(::std::vector<Player*> const& groupMembers, MovementFormationType formation = MovementFormationType::DUNGEON) override;
    bool LeaveFormation() override;
    bool ChangeFormation(MovementFormationType newFormation) override;
    bool SetFormationLeader(Player* leader) override;
    Player* GetFormationLeader() const override;
    void UpdateFormation(uint32 diff) override;
    bool ExecuteFormationCommand(FormationCommand const& command) override;
    bool MoveFormationToPosition(Position const& targetPos, float orientation = 0.0f) override;
    bool AdjustFormationForCombat(::std::vector<Unit*> const& threats) override;
    bool AddFormationMember(Player* player, FormationRole role = FormationRole::SUPPORT) override;
    bool RemoveFormationMember(Player* player) override;
    bool ChangeFormationMemberRole(Player* player, FormationRole newRole) override;
    FormationMember* GetFormationMember(Player* player) override;
    ::std::vector<FormationMember> GetAllFormationMembers() const override;
    Position CalculateFormationPosition(FormationRole role, uint32 memberIndex) override;
    ::std::vector<Position> CalculateAllFormationPositions() override;
    Position GetAssignedFormationPosition() const override;
    bool IsInFormationPosition(float tolerance = 2.0f) const override;
    FormationIntegrity AssessFormationIntegrity() override;
    float CalculateCohesionLevel() override;
    ::std::vector<Player*> GetOutOfPositionMembers(float tolerance = 3.0f) override;
    bool RequiresReformation() override;
    void CoordinateFormationMovement(Position const& destination) override;
    void MaintainFormationDuringMovement() override;
    bool CanMoveWithoutBreakingFormation(Position const& newPos) override;
    Position AdjustMovementForFormation(Position const& intendedPos) override;
    void TransitionToCombatFormation(::std::vector<Unit*> const& enemies) override;
    void TransitionToTravelFormation() override;
    void AdjustForThreatSpread(::std::vector<Unit*> const& threats) override;
    void HandleFormationBreakage() override;
    MovementFormationType DetermineOptimalFormation(::std::vector<Player*> const& members) override;
    FormationConfig GetFormationConfig(MovementFormationType formation) override;
    void SetFormationConfig(MovementFormationType formation, FormationConfig const& config) override;
    void AdjustFormationForTerrain() override;
    void AdjustFormationForObstacles(::std::vector<Position> const& obstacles) override;
    void AdjustFormationForGroupSize() override;
    void HandleMemberDisconnection(Player* disconnectedMember) override;
    MovementFormationType GetCurrentFormation() const override;
    FormationMovementState GetFormationMovementState() const override;
    bool IsFormationLeader() const override;
    bool IsInFormation() const override;
    uint32 GetFormationMemberCount() const override;
    void SetFormationUpdateInterval(uint32 intervalMs) override;
    uint32 GetFormationUpdateInterval() const override;
    void SetCohesionRadius(float radius) override;
    float GetCohesionRadius() const override;
    void SetFormationSpacing(float spacing) override;
    float GetFormationSpacing() const override;
    FormationMetrics const& GetFormationMetrics() const override;
    void ResetFormationMetrics() override;
    void EnableAdaptiveFormations(bool enable) override;
    bool IsAdaptiveFormationsEnabled() const override;
    void SetFormationPriority(uint32 priority) override;
    uint32 GetFormationPriority() const override;
    void ActivateEmergencyScatter() override;
    void DeactivateEmergencyScatter() override;
    bool IsEmergencyScatterActive() const override;
    void HandleEmergencyRegroup(Position const& rallyPoint) override;

    // ========================================================================
    // POSITION MODULE INTERFACE
    // ========================================================================

    MovementResult UpdatePosition(MovementContext const& context) override;
    MovementResult FindOptimalPosition(MovementContext const& context) override;
    MovementResult ExecuteMovement(Position const& targetPos, MovementPriority priority) override;
    PositionInfo EvaluatePosition(Position const& pos, MovementContext const& context) override;
    ::std::vector<PositionInfo> EvaluatePositions(::std::vector<Position> const& positions, MovementContext const& context) override;
    ::std::vector<Position> GenerateCandidatePositions(MovementContext const& context) override;
    Position FindRangePosition(Unit* target, float minRange, float maxRange, float preferredAngle = 0.0f) override;
    Position FindMeleePosition(Unit* target, bool preferBehind = true) override;
    Position FindRangedPosition(Unit* target, float preferredRange = 25.0f) override;
    Position FindHealingPosition(::std::vector<Player*> const& allies) override;
    Position FindKitingPosition(Unit* threat, float minDistance = 15.0f) override;
    Position FindTankPosition(Unit* target) override;
    Position FindDpsPosition(Unit* target, PositionType type = PositionType::MELEE_COMBAT) override;
    Position FindHealerPosition(::std::vector<Player*> const& groupMembers) override;
    Position FindSupportPosition(::std::vector<Player*> const& groupMembers) override;
    bool IsPositionSafe(Position const& pos, MovementContext const& context) override;
    bool IsInDangerZone(Position const& pos) override;
    Position FindSafePosition(Position const& fromPos, float minDistance = 10.0f) override;
    Position FindEscapePosition(::std::vector<Unit*> const& threats) override;
    void RegisterAoEZone(AoEZone const& zone) override;
    void UpdateAoEZones(uint32 currentTime) override;
    void ClearExpiredZones(uint32 currentTime) override;
    ::std::vector<AoEZone> GetActiveZones() const override;
    bool ValidatePosition(Position const& pos, PositionValidation flags) override;
    bool HasLineOfSight(Position const& from, Position const& to) override;
    bool IsWalkable(Position const& pos) override;
    float CalculateMovementCost(Position const& from, Position const& to) override;
    Position FindFormationPositionForRole(::std::vector<Player*> const& groupMembers, PositionType formationType) override;
    bool ShouldMaintainGroupProximity() override;
    float GetOptimalGroupDistance(uint8 role) override;
    bool ShouldStrafe(Unit* target) override;
    bool ShouldCircleStrafe(Unit* target) override;
    Position CalculateStrafePosition(Unit* target, bool strafeLeft = true) override;
    Position PredictTargetPosition(Unit* target, float timeAhead) override;
    void SetPositionWeights(PositionWeights const& weights) override;
    PositionWeights const& GetPositionWeights() const override;
    void SetPositionUpdateInterval(uint32 intervalMs) override;
    uint32 GetPositionUpdateInterval() const override;
    PositionMetrics const& GetPositionMetrics() const override;
    void ResetPositionMetrics() override;
    MovementResult HandleEmergencyMovement(MovementContext const& context) override;
    bool IsInEmergencyPosition() override;
    Position FindEmergencyEscapePosition() override;
    void RecordPositionSuccess(Position const& pos, PositionType type) override;
    void RecordPositionFailure(Position const& pos, ::std::string const& reason) override;
    float GetPositionSuccessRate(Position const& pos, float radius = 5.0f) override;

    // ========================================================================
    // UNIFIED OPERATIONS
    // ========================================================================

    void CoordinateCompleteMovement(Player* bot, MovementContext const& context) override;
    ::std::string GetMovementRecommendation(Player* bot, MovementContext const& context) override;
    void OptimizeBotMovement(Player* bot) override;
    ::std::string GetMovementStatistics() const override;

private:
    // ========================================================================
    // INTERNAL MODULES
    // ========================================================================

    /**
     * @brief Arbiter module - movement request arbitration
     */
    class ArbiterModule
    {
    public:
        explicit ArbiterModule(Player* bot);

        // Delegates to MovementArbiter instance
        bool RequestMovement(MovementRequest const& request);
        void ClearPendingRequests();
        void StopMovement();
        void Update(uint32 diff);
        MovementArbiterStatistics const& GetStatistics() const;
        void ResetStatistics();
        ::std::string GetDiagnosticString() const;
        void LogStatistics() const;
        MovementArbiterConfig GetConfig() const;
        void SetConfig(MovementArbiterConfig const& config);
        void SetDiagnosticLogging(bool enable);
        uint32 GetPendingRequestCount() const;
        bool HasPendingRequests() const;

        // Metrics accessors
        uint64 GetRequestsProcessed() const { return _requestsProcessed.load(); }

    private:
        ::std::unique_ptr<MovementArbiter> _arbiter;
        ::std::atomic<uint64> _requestsProcessed{0};
    };

    /**
     * @brief Pathfinding module - path calculation and caching
     */
    class PathfindingModule
    {
    public:
        PathfindingModule();

        // Delegates to PathfindingAdapter singleton
        bool Initialize(uint32 cacheSize, uint32 cacheDuration);
        void Shutdown();
        bool CalculatePath(Player* bot, Position const& destination, MovementPath& path, bool forceDirect);
        bool CalculatePathToUnit(Player* bot, Unit* target, MovementPath& path, float range);
        bool CalculateFormationPath(Player* bot, Unit* leader, Position const& offset, MovementPath& path);
        bool CalculateFleePath(Player* bot, Unit* threat, float distance, MovementPath& path);
        bool HasCachedPath(Player* bot, Position const& destination) const;
        bool GetCachedPath(Player* bot, Position const& destination, MovementPath& path) const;
        void ClearCache(Player* bot);
        void ClearAllCache();
        void SetPathParameters(uint32 maxNodes, float straightDistance, float maxSearchDistance);
        void EnableSmoothing(bool enable);
        void EnableCaching(bool enable);
        void SetCacheParameters(uint32 maxSize, uint32 duration);
        void GetCacheStatistics(uint32& hits, uint32& misses, uint32& evictions) const;
        void GetPathStatistics(uint32& totalPaths, uint32& avgTime, uint32& maxTime) const;
        void ResetStatistics();
        bool IsWalkablePosition(Map* map, Position const& position) const;
        bool GetNearestWalkablePosition(Map* map, Position const& position, Position& walkable, float searchRange) const;

        // Metrics accessors
        uint64 GetPathsCalculated() const { return _pathsCalculated.load(); }

    private:
        ::std::unique_ptr<PathfindingAdapter> _adapter;
        ::std::atomic<uint64> _pathsCalculated{0};
    };

    /**
     * @brief Formation module - group formation management
     */
    class FormationModule
    {
    public:
        explicit FormationModule(Player* bot);

        // Delegates to FormationManager instance
        bool JoinFormation(::std::vector<Player*> const& groupMembers, MovementFormationType formation);
        bool LeaveFormation();
        bool ChangeFormation(MovementFormationType newFormation);
        bool SetFormationLeader(Player* leader);
        Player* GetFormationLeader() const;
        void UpdateFormation(uint32 diff);
        bool ExecuteFormationCommand(FormationCommand const& command);
        bool MoveFormationToPosition(Position const& targetPos, float orientation);
        bool AdjustFormationForCombat(::std::vector<Unit*> const& threats);
        bool AddMember(Player* player, FormationRole role);
        bool RemoveMember(Player* player);
        bool ChangeMemberRole(Player* player, FormationRole newRole);
        FormationMember* GetMember(Player* player);
        ::std::vector<FormationMember> GetAllMembers() const;
        Position CalculateFormationPosition(FormationRole role, uint32 memberIndex);
        ::std::vector<Position> CalculateAllFormationPositions();
        Position GetAssignedPosition() const;
        bool IsInFormationPosition(float tolerance) const;
        FormationIntegrity AssessIntegrity();
        float CalculateCohesion();
        ::std::vector<Player*> GetOutOfPositionMembers(float tolerance);
        bool RequiresReformation();
        void CoordinateMovement(Position const& destination);
        void MaintainFormationDuringMovement();
        bool CanMoveWithoutBreaking(Position const& newPos);
        Position AdjustMovementForFormation(Position const& intendedPos);
        void TransitionToCombatFormation(::std::vector<Unit*> const& enemies);
        void TransitionToTravelFormation();
        void AdjustForThreatSpread(::std::vector<Unit*> const& threats);
        void HandleBreakage();
        MovementFormationType DetermineOptimalFormation(::std::vector<Player*> const& members);
        FormationConfig GetConfig(MovementFormationType formation);
        void SetConfig(MovementFormationType formation, FormationConfig const& config);
        void AdjustForTerrain();
        void AdjustForObstacles(::std::vector<Position> const& obstacles);
        void AdjustForGroupSize();
        void HandleMemberDisconnection(Player* disconnectedMember);
        MovementFormationType GetCurrentFormation() const;
        FormationMovementState GetMovementState() const;
        bool IsLeader() const;
        bool IsInFormation() const;
        uint32 GetMemberCount() const;
        void SetUpdateInterval(uint32 intervalMs);
        uint32 GetUpdateInterval() const;
        void SetCohesionRadius(float radius);
        float GetCohesionRadius() const;
        void SetFormationSpacing(float spacing);
        float GetFormationSpacing() const;
        FormationMetrics const& GetMetrics() const;
        void ResetMetrics();
        void EnableAdaptive(bool enable);
        bool IsAdaptiveEnabled() const;
        void SetPriority(uint32 priority);
        uint32 GetPriority() const;
        void ActivateEmergencyScatter();
        void DeactivateEmergencyScatter();
        bool IsEmergencyScatterActive() const;
        void HandleEmergencyRegroup(Position const& rallyPoint);

        // Metrics accessors
        uint64 GetFormationsExecuted() const { return _formationsExecuted.load(); }

    private:
        ::std::unique_ptr<FormationManager> _manager;
        ::std::atomic<uint64> _formationsExecuted{0};
    };

    /**
     * @brief Position module - combat positioning and tactical movement
     */
    class PositionModule
    {
    public:
        PositionModule(Player* bot, BotThreatManager* threatManager);

        // Delegates to PositionManager instance
        MovementResult UpdatePosition(MovementContext const& context);
        PositionMovementResult FindOptimalPosition(MovementContext const& context);
        MovementResult ExecuteMovement(Position const& targetPos, MovementPriority priority);
        PositionInfo EvaluatePosition(Position const& pos, MovementContext const& context);
        ::std::vector<PositionInfo> EvaluatePositions(::std::vector<Position> const& positions, MovementContext const& context);
        ::std::vector<Position> GenerateCandidatePositions(MovementContext const& context);
        Position FindRangePosition(Unit* target, float minRange, float maxRange, float preferredAngle);
        Position FindMeleePosition(Unit* target, bool preferBehind);
        Position FindRangedPosition(Unit* target, float preferredRange);
        Position FindHealingPosition(::std::vector<Player*> const& allies);
        Position FindKitingPosition(Unit* threat, float minDistance);
        Position FindTankPosition(Unit* target);
        Position FindDpsPosition(Unit* target, PositionType type);
        Position FindHealerPosition(::std::vector<Player*> const& groupMembers);
        Position FindSupportPosition(::std::vector<Player*> const& groupMembers);
        bool IsPositionSafe(Position const& pos, MovementContext const& context);
        bool IsInDangerZone(Position const& pos);
        Position FindSafePosition(Position const& fromPos, float minDistance);
        Position FindEscapePosition(::std::vector<Unit*> const& threats);
        void RegisterAoEZone(AoEZone const& zone);
        void UpdateAoEZones(uint32 currentTime);
        void ClearExpiredZones(uint32 currentTime);
        ::std::vector<AoEZone> GetActiveZones() const;
        bool ValidatePosition(Position const& pos, PositionValidation flags);
        bool HasLineOfSight(Position const& from, Position const& to);
        bool IsWalkable(Position const& pos);
        float CalculateMovementCost(Position const& from, Position const& to);
        Position FindFormationPosition(::std::vector<Player*> const& groupMembers, PositionType formationType);
        bool ShouldMaintainGroupProximity();
        float GetOptimalGroupDistance(uint8 role);
        bool ShouldStrafe(Unit* target);
        bool ShouldCircleStrafe(Unit* target);
        Position CalculateStrafePosition(Unit* target, bool strafeLeft);
        Position PredictTargetPosition(Unit* target, float timeAhead);
        void SetWeights(PositionWeights const& weights);
        PositionWeights const& GetWeights() const;
        void SetUpdateInterval(uint32 intervalMs);
        uint32 GetUpdateInterval() const;
        PositionMetrics const& GetMetrics() const;
        void ResetMetrics();
        MovementResult HandleEmergencyMovement(MovementContext const& context);
        bool IsInEmergencyPosition();
        Position FindEmergencyEscapePosition();
        void RecordPositionSuccess(Position const& pos, PositionType type);
        void RecordPositionFailure(Position const& pos, ::std::string const& reason);
        float GetPositionSuccessRate(Position const& pos, float radius);

        // Metrics accessors
        uint64 GetPositionsEvaluated() const { return _positionsEvaluated.load(); }

    private:
        ::std::unique_ptr<PositionManager> _manager;
        ::std::atomic<uint64> _positionsEvaluated{0};
    };

    // Module instances
    ::std::unique_ptr<ArbiterModule> _arbiter;
    ::std::unique_ptr<PathfindingModule> _pathfinding;
    ::std::unique_ptr<FormationModule> _formation;
    ::std::unique_ptr<PositionModule> _position;

    // Bot reference
    Player* _bot;

    // Global mutex for unified operations
    mutable Playerbot::OrderedMutex<Playerbot::LockOrder::MOVEMENT_ARBITER> _mutex;

    // Statistics
    ::std::atomic<uint64> _totalOperations{0};
    ::std::atomic<uint64> _totalProcessingTimeMs{0};
};

} // namespace Playerbot
