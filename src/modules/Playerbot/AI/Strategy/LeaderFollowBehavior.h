/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_LEADER_FOLLOW_BEHAVIOR_H
#define PLAYERBOT_LEADER_FOLLOW_BEHAVIOR_H

#include "Strategy.h"
#include "ObjectGuid.h"
#include <chrono>
#include <memory>

// Forward declarations
class Player;
class Unit;
class Group;

namespace Playerbot
{

// Forward declarations
class BotAI;
class GroupCombatTrigger;
class TargetAssistAction;

/**
 * @class LeaderFollowBehavior
 * @brief Strategy for following group leader and engaging in coordinated combat
 *
 * This strategy combines leader following, formation management, and combat
 * coordination to create effective group behavior for bots. It integrates
 * GroupCombatTrigger and TargetAssistAction for synchronized group combat.
 *
 * Performance Requirements:
 * - Formation update: <500ms interval
 * - Combat response: <3 seconds from leader engagement
 * - Memory usage: <1MB per bot for strategy
 * - CPU usage: <0.02% per bot
 */
class TC_GAME_API LeaderFollowBehavior : public Strategy
{
public:
    /**
     * Constructor
     */
    LeaderFollowBehavior();

    /**
     * Destructor
     */
    virtual ~LeaderFollowBehavior() = default;

    // Strategy interface implementation
    /**
     * Initialize strategy actions
     */
    void InitializeActions() override;

    /**
     * Initialize strategy triggers
     */
    void InitializeTriggers() override;

    /**
     * Initialize strategy values
     */
    void InitializeValues() override;

    /**
     * Calculate strategy relevance for AI
     * @param ai The bot AI instance
     * @return Relevance score 0.0-1.0
     */
    float GetRelevance(BotAI* ai) const override;

    /**
     * Calculate detailed relevance scores
     * @param ai The bot AI instance
     * @return StrategyRelevance with category scores
     */
    StrategyRelevance CalculateRelevance(BotAI* ai) const override;

    /**
     * Check if strategy is active
     * @param ai The bot AI instance
     * @return true if strategy should be active
     */
    bool IsActive(BotAI* ai) const override;

    /**
     * Called when strategy is activated
     * @param ai The bot AI instance
     */
    void OnActivate(BotAI* ai) override;

    /**
     * Called when strategy is deactivated
     * @param ai The bot AI instance
     */
    void OnDeactivate(BotAI* ai) override;

    // Leader following methods
    /**
     * Get the group leader
     * @param bot The bot player
     * @return The group leader or nullptr
     */
    Player* GetLeader(Player* bot) const;

    /**
     * Check if bot should follow leader
     * @param bot The bot player
     * @param leader The group leader
     * @return true if bot should follow
     */
    bool ShouldFollowLeader(Player* bot, Player* leader) const;

    /**
     * Calculate follow position based on formation
     * @param bot The bot player
     * @param leader The group leader
     * @param x Output X coordinate
     * @param y Output Y coordinate
     * @param z Output Z coordinate
     * @return true if position was calculated
     */
    bool CalculateFollowPosition(Player* bot, Player* leader, float& x, float& y, float& z) const;

    /**
     * Get follow distance based on role
     * @param bot The bot player
     * @return Follow distance in yards
     */
    float GetFollowDistance(Player* bot) const;

    // Formation management
    enum class FormationType
    {
        SINGLE_FILE,    // Line formation
        SPREAD,         // Spread out formation
        TIGHT,          // Close formation
        COMBAT_SPREAD,  // Combat positioning
        DEFENSIVE       // Defensive circle
    };

    /**
     * Set group formation type
     * @param type Formation type to use
     */
    void SetFormationType(FormationType type) { _formationType = type; }

    /**
     * Get current formation type
     * @return Current formation type
     */
    FormationType GetFormationType() const { return _formationType; }

    /**
     * Calculate formation position for bot
     * @param bot The bot player
     * @param group The bot's group
     * @param index Bot's position in formation
     * @param x Output X coordinate
     * @param y Output Y coordinate
     * @param z Output Z coordinate
     * @return true if position was calculated
     */
    bool GetFormationPosition(Player* bot, Group* group, uint32 index, float& x, float& y, float& z) const;

    // Combat coordination
    /**
     * Check if group is in combat
     * @param group The group to check
     * @return true if any member is in combat
     */
    bool IsGroupInCombat(Group* group) const;

    /**
     * Get group's primary target
     * @param group The group to check
     * @return The main combat target or nullptr
     */
    Unit* GetGroupTarget(Group* group) const;

    /**
     * Check if bot should engage combat
     * @param bot The bot player
     * @return true if bot should enter combat
     */
    bool ShouldEngageCombat(Player* bot) const;

    /**
     * Handle combat engagement
     * @param ai The bot AI instance
     * @param target The target to engage
     * @return true if engagement successful
     */
    bool EngageCombat(BotAI* ai, Unit* target);

    // Movement control
    /**
     * Update bot movement to follow leader
     * @param ai The bot AI instance
     * @param diff Time since last update in ms
     */
    void UpdateMovement(BotAI* ai, uint32 diff);

    /**
     * Check if bot is in correct position
     * @param bot The bot player
     * @return true if bot is positioned correctly
     */
    bool IsInPosition(Player* bot) const;

    /**
     * Stop following behavior
     * @param ai The bot AI instance
     */
    void StopFollowing(BotAI* ai);

    // Configuration
    /**
     * Set maximum follow distance
     * @param distance Maximum distance in yards
     */
    void SetMaxFollowDistance(float distance) { _maxFollowDistance = distance; }

    /**
     * Set minimum follow distance
     * @param distance Minimum distance in yards
     */
    void SetMinFollowDistance(float distance) { _minFollowDistance = distance; }

    /**
     * Enable or disable combat assistance
     * @param enable true to enable combat assistance
     */
    void SetCombatAssistEnabled(bool enable) { _combatAssistEnabled = enable; }

    /**
     * Set movement update interval
     * @param intervalMs Update interval in milliseconds
     */
    void SetUpdateInterval(uint32 intervalMs) { _updateIntervalMs = intervalMs; }

    // Statistics and monitoring
    struct BehaviorStats
    {
        uint32 followCommands = 0;
        uint32 combatEngagements = 0;
        uint32 formationAdjustments = 0;
        uint32 targetAssists = 0;
        float averageFollowDistance = 0.0f;
        std::chrono::milliseconds averageResponseTime{0};
        std::chrono::steady_clock::time_point lastUpdate;
    };

    /**
     * Get behavior statistics
     * @return Current statistics
     */
    BehaviorStats const& GetStats() const { return _stats; }

    /**
     * Reset behavior statistics
     */
    void ResetStats() { _stats = BehaviorStats{}; }

private:
    // Internal methods
    /**
     * Update formation positions
     * @param ai The bot AI instance
     */
    void UpdateFormation(BotAI* ai);

    /**
     * Check if movement is needed
     * @param bot The bot player
     * @param targetX Target X coordinate
     * @param targetY Target Y coordinate
     * @param targetZ Target Z coordinate
     * @return true if bot needs to move
     */
    bool NeedsMovement(Player* bot, float targetX, float targetY, float targetZ) const;

    /**
     * Calculate role-based position offset
     * @param bot The bot player
     * @param baseAngle Base angle from leader
     * @param distance Distance from leader
     * @return Adjusted angle for role
     */
    float GetRolePositionOffset(Player* bot, float baseAngle, float& distance) const;

    /**
     * Check if position is safe
     * @param bot The bot player
     * @param x X coordinate to check
     * @param y Y coordinate to check
     * @param z Z coordinate to check
     * @return true if position is safe
     */
    bool IsPositionSafe(Player* bot, float x, float y, float z) const;

    /**
     * Log behavior event
     * @param event Event description
     * @param ai The bot AI involved
     */
    void LogBehaviorEvent(std::string const& event, BotAI* ai) const;

    /**
     * Update statistics
     * @param eventType Type of event to track
     * @param responseTime Response time for event
     */
    void UpdateStatistics(std::string const& eventType, std::chrono::milliseconds responseTime);

    // Member variables
    FormationType _formationType = FormationType::SPREAD;              // Current formation type
    std::shared_ptr<GroupCombatTrigger> _combatTrigger;               // Combat detection trigger
    std::shared_ptr<TargetAssistAction> _assistAction;                // Target assist action

    // Configuration
    float _maxFollowDistance = 50.0f;                                 // Maximum follow distance
    float _minFollowDistance = 5.0f;                                  // Minimum follow distance
    float _combatFollowDistance = 15.0f;                              // Follow distance in combat
    bool _combatAssistEnabled = true;                                 // Enable combat assistance
    bool _formationEnabled = true;                                    // Enable formation keeping
    uint32 _updateIntervalMs = 500;                                   // Movement update interval

    // State tracking
    ObjectGuid _currentLeader;                                        // Current leader being followed
    std::chrono::steady_clock::time_point _lastMovementUpdate;        // Last movement update time
    std::chrono::steady_clock::time_point _lastFormationCheck;        // Last formation check time
    bool _isFollowing = false;                                        // Currently following
    bool _inCombatFormation = false;                                  // Using combat formation

    // Statistics
    mutable BehaviorStats _stats;                                     // Performance statistics

    // Constants
    static constexpr float POSITION_TOLERANCE = 3.0f;                // Position accuracy tolerance
    static constexpr float FORMATION_SPACING = 5.0f;                 // Default formation spacing
    static constexpr float COMBAT_FORMATION_SPACING = 8.0f;          // Combat formation spacing
    static constexpr uint32 FORMATION_UPDATE_INTERVAL = 1000;         // Formation update interval
    static constexpr float TANK_FOLLOW_DISTANCE = 5.0f;              // Tank follow distance
    static constexpr float HEALER_FOLLOW_DISTANCE = 20.0f;           // Healer follow distance
    static constexpr float DPS_FOLLOW_DISTANCE = 10.0f;              // DPS follow distance
};

} // namespace Playerbot

#endif // PLAYERBOT_LEADER_FOLLOW_BEHAVIOR_H