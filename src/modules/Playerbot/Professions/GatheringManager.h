/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef TRINITYCORE_GATHERING_MANAGER_H
#define TRINITYCORE_GATHERING_MANAGER_H

#include "Define.h"
#include "AI/BehaviorManager.h"
#include "ObjectGuid.h"
#include "Position.h"
#include "SharedDefines.h"
#include <atomic>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <mutex>

class Player;
class GameObject;
class Creature;
class Spell;

namespace Playerbot
{
    class BotAI;

    /**
     * @enum GatheringNodeType
     * @brief Types of gathering nodes for different professions
     */
    enum class GatheringNodeType : uint8
    {
        NONE = 0,
        MINING_VEIN,        // Copper, Tin, Iron, etc. veins
        HERB_NODE,          // Peacebloom, Silverleaf, etc. herbs
        FISHING_POOL,       // Schools of fish
        CREATURE_CORPSE     // Skinnable creature bodies
    };

    /**
     * @struct GatheringNode
     * @brief Information about a detected gathering node
     */
    struct GatheringNode
    {
        ObjectGuid guid;
        GatheringNodeType nodeType;
        uint32 entry;               // GameObject or Creature entry
        Position position;
        uint16 requiredSkill;        // Minimum skill to gather
        uint32 detectionTime;        // When node was detected
        bool isActive;               // Still harvestable
        float distance;              // Distance from bot
        uint32 attemptCount;         // Number of gathering attempts
        uint32 lastAttemptTime;      // Time of last gathering attempt

        GatheringNode()
            : nodeType(GatheringNodeType::NONE), entry(0), requiredSkill(0),
              detectionTime(0), isActive(true), distance(0.0f),
              attemptCount(0), lastAttemptTime(0) {}
    };

    /**
     * @struct GatheringStatistics
     * @brief Gathering statistics per bot
     */
    struct GatheringStatistics
    {
        uint32 nodesGathered = 0;
        uint32 itemsLooted = 0;
        uint32 skillPointsGained = 0;
        uint32 failedAttempts = 0;
        uint32 timeSpentGathering = 0;  // Milliseconds
        uint32 distanceTraveled = 0;    // Yards
        std::chrono::steady_clock::time_point lastReset;

        GatheringStatistics() : lastReset(std::chrono::steady_clock::now()) {}

        void Reset()
        {
            nodesGathered = 0;
            itemsLooted = 0;
            skillPointsGained = 0;
            failedAttempts = 0;
            timeSpentGathering = 0;
            distanceTraveled = 0;
            lastReset = std::chrono::steady_clock::now();
        }
    };

    /**
     * @class GatheringManager
     * @brief Manages gathering professions for bots (Mining, Herbalism, Skinning, Fishing)
     *
     * Inherits from BehaviorManager for throttled updates and performance optimization.
     * Handles:
     * - Node detection and tracking
     * - Pathfinding to gathering nodes
     * - Gathering spell casting
     * - Loot collection
     * - Skill-up tracking
     *
     * Update interval: 1000ms (1 second)
     */
    class TC_GAME_API GatheringManager : public BehaviorManager
    {
    public:
        explicit GatheringManager(Player* bot, BotAI* ai);
        ~GatheringManager() override = default;

        // ========================================================================
        // FAST STATE QUERIES (<0.001ms atomic reads)
        // ========================================================================

        /**
         * @brief Check if bot is currently gathering
         * @return true if actively gathering, false otherwise
         */
        bool IsGathering() const { return _isGathering.load(std::memory_order_acquire); }

        /**
         * @brief Check if gathering nodes are nearby
         * @return true if nodes detected, false otherwise
         */
        bool HasNearbyResources() const { return _hasNearbyResources.load(std::memory_order_acquire); }

        /**
         * @brief Get number of detected nodes
         * @return Count of detected gathering nodes
         */
        uint32 GetDetectedNodeCount() const { return _detectedNodeCount.load(std::memory_order_acquire); }

        /**
         * @brief Check if bot is moving to a node
         * @return true if pathing to node, false otherwise
         */
        bool IsMovingToNode() const { return _isMovingToNode.load(std::memory_order_acquire); }

        // ========================================================================
        // NODE DETECTION
        // ========================================================================

        /**
         * @brief Scan for nearby gathering nodes
         * @param range Detection range in yards
         * @return Vector of detected nodes sorted by distance
         */
        std::vector<GatheringNode> ScanForNodes(float range = 40.0f);

        /**
         * @brief Find nearest gathering node
         * @param nodeType Specific node type to find, or NONE for any
         * @return Pointer to nearest node, nullptr if none found
         */
        GatheringNode const* FindNearestNode(GatheringNodeType nodeType = GatheringNodeType::NONE) const;

        /**
         * @brief Check if player can gather from node
         * @param node Node to check
         * @return true if player has required skill, false otherwise
         */
        bool CanGatherFromNode(GatheringNode const& node) const;

        /**
         * @brief Get required skill level for node
         * @param node Node to check
         * @return Required skill level
         */
        uint16 GetRequiredSkillForNode(GatheringNode const& node) const;

        // ========================================================================
        // GATHERING ACTIONS
        // ========================================================================

        /**
         * @brief Attempt to gather from node
         * @param node Node to gather from
         * @return true if gathering initiated, false otherwise
         */
        bool GatherFromNode(GatheringNode const& node);

        /**
         * @brief Cast gathering spell on node
         * @param node Target node
         * @return true if spell cast started, false otherwise
         */
        bool CastGatheringSpell(GatheringNode const& node);

        /**
         * @brief Loot gathered items from node
         * @param gameObject Node game object
         * @return true if looting successful, false otherwise
         */
        bool LootNode(GameObject* gameObject);

        /**
         * @brief Skin creature corpse
         * @param creature Creature to skin
         * @return true if skinning successful, false otherwise
         */
        bool SkinCreature(Creature* creature);

        /**
         * @brief Get gathering spell ID for profession
         * @param nodeType Type of node to gather
         * @param skillLevel Current skill level
         * @return Spell ID to cast, 0 if none
         */
        uint32 GetGatheringSpellId(GatheringNodeType nodeType, uint16 skillLevel) const;

        // ========================================================================
        // PATHFINDING INTEGRATION
        // ========================================================================

        /**
         * @brief Move bot to gathering node
         * @param node Target node
         * @return true if movement initiated, false otherwise
         */
        bool PathToNode(GatheringNode const& node);

        /**
         * @brief Check if bot is in range to gather
         * @param node Target node
         * @return true if in gathering range, false otherwise
         */
        bool IsInGatheringRange(GatheringNode const& node) const;

        /**
         * @brief Calculate distance to node
         * @param node Target node
         * @return Distance in yards
         */
        float GetDistanceToNode(GatheringNode const& node) const;

        /**
         * @brief Stop current gathering action
         */
        void StopGathering();

        // ========================================================================
        // CONFIGURATION
        // ========================================================================

        /**
         * @brief Enable or disable gathering automation
         * @param enable true to enable, false to disable
         */
        void SetGatheringEnabled(bool enable) { _gatheringEnabled = enable; }

        /**
         * @brief Check if gathering is enabled
         * @return true if enabled, false otherwise
         */
        bool IsGatheringEnabled() const { return _gatheringEnabled; }

        /**
         * @brief Set detection range for nodes
         * @param range Range in yards
         */
        void SetDetectionRange(float range) { _detectionRange = std::min(range, 100.0f); }

        /**
         * @brief Get current detection range
         * @return Range in yards
         */
        float GetDetectionRange() const { return _detectionRange; }

        /**
         * @brief Enable or disable specific gathering profession
         * @param nodeType Node type to enable/disable
         * @param enable true to enable, false to disable
         */
        void SetProfessionEnabled(GatheringNodeType nodeType, bool enable);

        /**
         * @brief Check if specific profession is enabled
         * @param nodeType Node type to check
         * @return true if enabled, false otherwise
         */
        bool IsProfessionEnabled(GatheringNodeType nodeType) const;

        /**
         * @brief Set whether to prioritize skill-ups
         * @param prioritize true to prioritize nodes that give skill-ups
         */
        void SetPrioritizeSkillUps(bool prioritize) { _prioritizeSkillUps = prioritize; }

        /**
         * @brief Set whether to gather while moving
         * @param enable true to gather during travel
         */
        void SetGatherWhileMoving(bool enable) { _gatherWhileMoving = enable; }

        // ========================================================================
        // STATISTICS
        // ========================================================================

        /**
         * @brief Get gathering statistics
         * @return Current gathering statistics
         */
        GatheringStatistics const& GetStatistics() const { return _statistics; }

        /**
         * @brief Reset gathering statistics
         */
        void ResetStatistics() { _statistics.Reset(); }

    protected:
        // ========================================================================
        // BEHAVIOR MANAGER INTERFACE
        // ========================================================================

        /**
         * @brief Main update method called every 1 second
         * @param elapsed Time since last update in milliseconds
         */
        void OnUpdate(uint32 elapsed) override;

        /**
         * @brief Initialize manager on first update
         * @return true if initialization successful, false to retry
         */
        bool OnInitialize() override;

        /**
         * @brief Cleanup on shutdown
         */
        void OnShutdown() override;

    private:
        // ========================================================================
        // INTERNAL GATHERING LOGIC
        // ========================================================================

        /**
         * @brief Detect mining veins in range
         * @param range Detection range
         * @return Vector of detected mining nodes
         */
        std::vector<GatheringNode> DetectMiningNodes(float range);

        /**
         * @brief Detect herb nodes in range
         * @param range Detection range
         * @return Vector of detected herb nodes
         */
        std::vector<GatheringNode> DetectHerbNodes(float range);

        /**
         * @brief Detect fishing pools in range
         * @param range Detection range
         * @return Vector of detected fishing nodes
         */
        std::vector<GatheringNode> DetectFishingPools(float range);

        /**
         * @brief Detect skinnable creatures in range
         * @param range Detection range
         * @return Vector of detected skinnable creatures
         */
        std::vector<GatheringNode> DetectSkinnableCreatures(float range);

        /**
         * @brief Convert GameObject to GatheringNode
         * @param gameObject GameObject to convert
         * @param nodeType Type of node
         * @return Gathering node information
         */
        GatheringNode CreateNodeInfo(GameObject* gameObject, GatheringNodeType nodeType);

        /**
         * @brief Convert Creature to GatheringNode
         * @param creature Creature to convert
         * @return Gathering node information
         */
        GatheringNode CreateNodeInfo(Creature* creature);

        /**
         * @brief Validate node is still active
         * @param node Node to validate
         * @return true if node still exists and is harvestable
         */
        bool ValidateNode(GatheringNode const& node);

        /**
         * @brief Calculate skill-up chance for node
         * @param node Target node
         * @return Skill-up chance (0.0 - 1.0)
         */
        float GetSkillUpChance(GatheringNode const& node) const;

        /**
         * @brief Handle gathering result
         * @param node Gathered node
         * @param success Whether gathering was successful
         */
        void HandleGatheringResult(GatheringNode const& node, bool success);

        /**
         * @brief Update detected nodes list
         */
        void UpdateDetectedNodes();

        /**
         * @brief Select best node to gather
         * @return Pointer to best node, nullptr if none suitable
         */
        GatheringNode const* SelectBestNode() const;

        /**
         * @brief Process current gathering action
         */
        void ProcessCurrentGathering();

        /**
         * @brief Clean up expired nodes
         */
        void CleanupExpiredNodes();

        /**
         * @brief Record gathering statistics
         * @param node Node that was gathered
         * @param success Whether gathering was successful
         */
        void RecordStatistics(GatheringNode const& node, bool success);

        /**
         * @brief Check if player has required profession
         * @param nodeType Node type to check
         * @return true if player has profession, false otherwise
         */
        bool HasProfession(GatheringNodeType nodeType) const;

        /**
         * @brief Get current skill level for profession
         * @param nodeType Node type to check
         * @return Current skill level, 0 if profession not learned
         */
        uint16 GetProfessionSkill(GatheringNodeType nodeType) const;

        /**
         * @brief Handle spell cast completion
         * @param spell Completed spell
         */
        void OnSpellCastComplete(Spell const* spell);

        // ========================================================================
        // STATE FLAGS (Atomic for fast queries)
        // ========================================================================

        std::atomic<bool> _isGathering{false};
        std::atomic<bool> _hasNearbyResources{false};
        std::atomic<bool> _isMovingToNode{false};
        std::atomic<uint32> _detectedNodeCount{0};

        // ========================================================================
        // CONFIGURATION
        // ========================================================================

        bool _gatheringEnabled = true;
        float _detectionRange = 40.0f;
        bool _prioritizeSkillUps = true;
        bool _gatherWhileMoving = true;
        bool _returnToPathAfterGather = true;

        // Profession-specific settings
        bool _gatherMining = true;
        bool _gatherHerbalism = true;
        bool _gatherSkinning = true;
        bool _gatherFishing = false;  // Requires special handling

        // ========================================================================
        // NODE TRACKING
        // ========================================================================

        std::vector<GatheringNode> _detectedNodes;
        GatheringNode const* _currentTarget = nullptr;
        std::chrono::steady_clock::time_point _lastScanTime;
        std::chrono::steady_clock::time_point _gatheringStartTime;

        // ========================================================================
        // GATHERING STATE
        // ========================================================================

        ObjectGuid _currentNodeGuid;
        uint32 _currentSpellId = 0;
        bool _isLooting = false;
        uint32 _gatheringAttempts = 0;
        Position _returnPosition;  // Position to return to after gathering

        // ========================================================================
        // STATISTICS
        // ========================================================================

        GatheringStatistics _statistics;

        // ========================================================================
        // CONSTANTS
        // ========================================================================

        static constexpr uint32 NODE_SCAN_INTERVAL = 3000;       // 3 seconds
        static constexpr uint32 GATHERING_CAST_TIME = 2000;      // 2 seconds
        static constexpr uint32 NODE_CACHE_DURATION = 30000;     // 30 seconds
        static constexpr uint32 MAX_GATHERING_ATTEMPTS = 3;      // Max attempts per node
        static constexpr float GATHERING_RANGE = 5.0f;           // Yards
        static constexpr float SKINNING_RANGE = 5.0f;            // Yards

        // Skill IDs for gathering professions
        static constexpr uint32 SKILL_MINING = 186;
        static constexpr uint32 SKILL_HERBALISM = 182;
        static constexpr uint32 SKILL_SKINNING = 393;
        static constexpr uint32 SKILL_FISHING = 356;

        // Basic gathering spell IDs (3.3.5a)
        static constexpr uint32 SPELL_MINING = 2575;
        static constexpr uint32 SPELL_HERB_GATHERING = 2366;
        static constexpr uint32 SPELL_SKINNING = 8613;
        static constexpr uint32 SPELL_FISHING = 7620;

        // ========================================================================
        // THREAD SAFETY
        // ========================================================================

        mutable std::mutex _nodeMutex;
    };

} // namespace Playerbot

#endif // TRINITYCORE_GATHERING_MANAGER_H