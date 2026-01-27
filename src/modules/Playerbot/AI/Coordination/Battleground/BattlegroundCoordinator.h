/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "BGState.h"
#include "Core/Events/ICombatEventSubscriber.h"
#include <memory>
#include <vector>
#include <map>

class Battleground;
class Player;

namespace Playerbot {

class ObjectiveManager;
class BGRoleManager;
class FlagCarrierManager;
class NodeController;
class BGStrategyEngine;

namespace Coordination::Battleground {
    class IBGScript;
    struct BGScriptEventData;
}

/**
 * @class BattlegroundCoordinator
 * @brief Coordinates AI bot behavior in battlegrounds
 *
 * The BattlegroundCoordinator manages all aspects of battleground play including:
 * - Objective tracking and prioritization
 * - Role assignment (FC, defense, offense, etc.)
 * - Flag carrier management (CTF maps)
 * - Node control (AB, BFG, etc.)
 * - Strategic decision making
 */
class BattlegroundCoordinator : public ICombatEventSubscriber
{
public:
    BattlegroundCoordinator(Battleground* bg, ::std::vector<Player*> bots);
    ~BattlegroundCoordinator();

    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    void Initialize();
    void Shutdown();
    void Update(uint32 diff);
    void Reset();

    // ========================================================================
    // STATE
    // ========================================================================

    BGState GetState() const { return _state; }
    BGType GetType() const { return _bgType; }
    bool IsActive() const { return _state == BGState::ACTIVE || _state == BGState::OVERTIME; }
    uint32 GetFaction() const { return _faction; }

    // ========================================================================
    // SCORE
    // ========================================================================

    const BGScoreInfo& GetScore() const { return _score; }
    const BGScoreInfo& GetScoreInfo() const { return _score; }  // Alias for GetScore
    bool IsWinning() const;
    float GetScoreAdvantage() const;
    uint32 GetTimeRemaining() const;
    float GetWinProbability() const;

    // ========================================================================
    // OBJECTIVES
    // ========================================================================

    ::std::vector<BGObjective> GetObjectives() const;
    BGObjective* GetObjective(uint32 objectiveId);
    const BGObjective* GetObjective(uint32 objectiveId) const;
    BGObjectiveState GetObjectiveState(uint32 objectiveId) const;
    BGObjective* GetNearestObjective(ObjectGuid player, ObjectiveType type = ObjectiveType::NODE) const;
    ::std::vector<BGObjective*> GetContestedObjectives() const;
    uint32 GetControlledObjectiveCount() const;
    uint32 GetEnemyControlledObjectiveCount() const;

    // ========================================================================
    // ROLE MANAGEMENT
    // ========================================================================

    BGRole GetBotRole(ObjectGuid bot) const;
    void AssignRole(ObjectGuid bot, BGRole role);
    void AssignToObjective(ObjectGuid bot, uint32 objectiveId);
    ::std::vector<ObjectGuid> GetBotsWithRole(BGRole role) const;
    uint32 GetRoleCount(BGRole role) const;

    // ========================================================================
    // FLAG MANAGEMENT (CTF MAPS)
    // ========================================================================

    bool IsCTFMap() const;
    bool HasFlag(ObjectGuid player) const;
    ObjectGuid GetFriendlyFC() const;
    ObjectGuid GetEnemyFC() const;
    const FlagInfo& GetFriendlyFlag() const;
    const FlagInfo& GetEnemyFlag() const;
    bool CanCaptureFlag() const;
    bool ShouldDropFlag() const;

    // ========================================================================
    // STRATEGIC COMMANDS
    // ========================================================================

    void CommandAttack(uint32 objectiveId);
    void CommandDefend(uint32 objectiveId);
    void CommandRecall();
    void CommandPush();
    void CommandRegroup();

    // ========================================================================
    // BOT QUERIES
    // ========================================================================

    BGObjective* GetAssignment(ObjectGuid bot) const;
    float GetDistanceToAssignment(ObjectGuid bot) const;
    bool ShouldContestObjective(ObjectGuid bot) const;
    bool ShouldRetreat(ObjectGuid bot) const;
    bool ShouldAssist(ObjectGuid bot, ObjectGuid ally) const;

    // ========================================================================
    // PLAYER TRACKING
    // ========================================================================

    const BGPlayer* GetBot(ObjectGuid guid) const;
    BGPlayer* GetBotMutable(ObjectGuid guid);
    ::std::vector<BGPlayer> GetAllBots() const;
    ::std::vector<BGPlayer> GetAliveBots() const;

    // ========================================================================
    // PLAYER ACCESS (for sub-managers)
    // ========================================================================

    /**
     * @brief Get a Player pointer from ObjectGuid
     * @param guid The player's GUID
     * @return Player pointer, or nullptr if not found/valid
     */
    Player* GetPlayer(ObjectGuid guid) const;

    /**
     * @brief Get all friendly player GUIDs (bots on our team)
     * @return Vector of ObjectGuids for friendly players
     */
    ::std::vector<ObjectGuid> GetFriendlyPlayers() const;

    /**
     * @brief Get all enemy player GUIDs
     * @return Vector of ObjectGuids for enemy players
     */
    ::std::vector<ObjectGuid> GetEnemyPlayers() const;

    /**
     * @brief Get the battleground instance
     * @return Pointer to the Battleground
     */
    Battleground* GetBattleground() const { return _battleground; }

    // ========================================================================
    // MATCH STATISTICS
    // ========================================================================

    const BGMatchStats& GetMatchStats() const { return _matchStats; }

    // ========================================================================
    // ICOMBATEVENTSUBSCRIBER
    // ========================================================================

    void OnCombatEvent(const CombatEvent& event) override;
    CombatEventType GetSubscribedEventTypes() const override;
    int32 GetEventPriority() const override { return 35; }

    // ========================================================================
    // SUB-MANAGER ACCESS
    // ========================================================================

    ObjectiveManager* GetObjectiveManager() const { return _objectiveManager.get(); }
    BGRoleManager* GetRoleManager() const { return _roleManager.get(); }
    FlagCarrierManager* GetFlagManager() const { return _flagManager.get(); }
    NodeController* GetNodeController() const { return _nodeController.get(); }
    BGStrategyEngine* GetStrategyEngine() const { return _strategyEngine.get(); }

    // ========================================================================
    // BG-SPECIFIC SCRIPT
    // ========================================================================

    Coordination::Battleground::IBGScript* GetScript() const { return _activeScript.get(); }
    void NotifyScriptEvent(const Coordination::Battleground::BGScriptEventData& event);

private:
    // ========================================================================
    // STATE
    // ========================================================================

    BGState _state = BGState::IDLE;
    BGType _bgType;
    BGScoreInfo _score;

    // ========================================================================
    // REFERENCES
    // ========================================================================

    Battleground* _battleground;
    ::std::vector<Player*> _managedBots;
    uint32 _faction;  // ALLIANCE or HORDE

    // ========================================================================
    // TRACKING
    // ========================================================================

    ::std::vector<BGObjective> _objectives;
    ::std::vector<BGPlayer> _bots;
    BGMatchStats _matchStats;

    uint32 _matchStartTime = 0;

    // ========================================================================
    // FLAGS (CTF MAPS)
    // ========================================================================

    FlagInfo _friendlyFlag;
    FlagInfo _enemyFlag;

    // ========================================================================
    // SUB-MANAGERS
    // ========================================================================

    ::std::unique_ptr<ObjectiveManager> _objectiveManager;
    ::std::unique_ptr<BGRoleManager> _roleManager;
    ::std::unique_ptr<FlagCarrierManager> _flagManager;
    ::std::unique_ptr<NodeController> _nodeController;
    ::std::unique_ptr<BGStrategyEngine> _strategyEngine;
    ::std::unique_ptr<Coordination::Battleground::IBGScript> _activeScript;

    // ========================================================================
    // STATE MACHINE
    // ========================================================================

    void UpdateState(uint32 diff);
    void TransitionTo(BGState newState);
    void OnStateEnter(BGState state);
    void OnStateExit(BGState state);

    // ========================================================================
    // BG-SPECIFIC INITIALIZATION
    // ========================================================================

    void DetectBGType();
    void InitializeWSG();
    void InitializeAB();
    void InitializeAV();
    void InitializeEOTS();
    void InitializeSOTA();
    void InitializeIOC();
    void InitializeTwinPeaks();
    void InitializeBFG();
    void InitializeSilvershardMines();
    void InitializeTempleOfKotmogu();
    void InitializeDeepwindGorge();

    // ========================================================================
    // EVENT HANDLERS
    // ========================================================================

    void HandleObjectiveCaptured(uint32 objectiveId, uint32 faction);
    void HandleObjectiveLost(uint32 objectiveId);
    void HandleFlagPickup(ObjectGuid player, bool isEnemyFlag);
    void HandleFlagDrop(ObjectGuid player);
    void HandleFlagCapture(ObjectGuid player);
    void HandleFlagReturn(ObjectGuid player);
    void HandlePlayerDied(ObjectGuid player, ObjectGuid killer);
    void HandlePlayerKill(ObjectGuid killer, ObjectGuid victim);

    // ========================================================================
    // STRATEGIC DECISIONS
    // ========================================================================

    void EvaluateStrategy(uint32 diff);
    void ReassignRoles();
    void UpdateObjectivePriorities();

    // ========================================================================
    // TRACKING UPDATES
    // ========================================================================

    void UpdateBotTracking(uint32 diff);
    void UpdateScoreTracking();
    void UpdateObjectiveTracking(uint32 diff);

    // ========================================================================
    // UTILITY
    // ========================================================================

    bool IsAlly(ObjectGuid player) const;
    bool IsEnemy(ObjectGuid player) const;
    bool IsFriendlyObjective(const BGObjective& objective) const;
    bool IsEnemyObjective(const BGObjective& objective) const;
};

} // namespace Playerbot
