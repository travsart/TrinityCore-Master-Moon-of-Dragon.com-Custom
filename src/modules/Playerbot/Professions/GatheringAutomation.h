/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * GATHERING AUTOMATION SYSTEM FOR PLAYERBOT
 *
 * This system provides comprehensive gathering profession automation:
 * - Mining: Detect and harvest ore/mineral veins
 * - Herbalism: Detect and harvest herb nodes
 * - Skinning: Detect and skin creature corpses
 * - Fishing: Detect fishing pools and cast fishing spells
 *
 * Architecture:
 * - GatheringAutomation: Core singleton managing all gathering operations
 * - Node detection using GAMEOBJECT_TYPE_GATHERING_NODE (type 50)
 * - Pathfinding integration for movement to nodes
 * - Spell casting for gathering actions
 * - Loot collection and inventory management
 *
 * Integration with Phase 1 & 2:
 * - Works with ProfessionManager for skill tracking
 * - Coordinates with FarmingCoordinator for leveling sync
 * - Provides materials for CraftingAutomation
 */

#pragma once

#include "Define.h"
#include "Player.h"
#include "GameObject.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "ProfessionManager.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace Playerbot
{

/**
 * @brief Gathering node types for different professions
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
 * @brief Information about a detected gathering node
 */
struct GatheringNodeInfo
{
    ObjectGuid guid;
    GatheringNodeType nodeType;
    uint32 gameObjectEntry;     // For mining/herbalism/fishing
    uint32 creatureEntry;       // For skinning
    Position position;
    uint16 requiredSkill;       // Minimum skill to gather
    ProfessionType profession;
    uint32 detectionTime;       // When node was detected
    bool isActive;              // Still harvestable
    float distance;             // Distance from bot

    GatheringNodeInfo()
        : nodeType(GatheringNodeType::NONE), gameObjectEntry(0), creatureEntry(0),
          requiredSkill(0), profession(ProfessionType::NONE), detectionTime(0),
          isActive(true), distance(0.0f) {}
};

/**
 * @brief Gathering automation profile per bot
 */
struct GatheringAutomationProfile
{
    bool autoGather = true;                 // Enable automatic gathering
    float detectionRange = 40.0f;           // Node detection range (yards)
    uint32 gatheringInterval = 2000;        // Time between gathering attempts (ms)
    bool prioritizeSkillUps = true;         // Prioritize nodes that give skill-ups
    bool gatherWhileMoving = true;          // Gather nodes while traveling
    bool returnToPathAfterGather = true;    // Return to original path after detour
    uint32 maxInventorySlots = 20;          // Reserve slots for gathered materials

    // Profession-specific settings
    bool gatherMining = true;
    bool gatherHerbalism = true;
    bool gatherSkinning = true;
    bool gatherFishing = false;             // Requires special handling

    GatheringAutomationProfile() = default;
};

/**
 * @brief Gathering statistics per bot
 */
struct GatheringStatistics
{
    std::atomic<uint32> nodesGathered{0};
    std::atomic<uint32> itemsLooted{0};
    std::atomic<uint32> skillPointsGained{0};
    std::atomic<uint32> failedAttempts{0};
    std::atomic<uint32> timeSpentGathering{0};  // Milliseconds
    std::atomic<uint32> distanceTraveled{0};    // Yards

    void Reset()
    {
        nodesGathered = 0;
        itemsLooted = 0;
        skillPointsGained = 0;
        failedAttempts = 0;
        timeSpentGathering = 0;
        distanceTraveled = 0;
    }
};

/**
 * @brief Complete gathering automation system for all gathering professions
 */
class TC_GAME_API GatheringAutomation
{
public:
    static GatheringAutomation* instance();

    // ============================================================================
    // CORE GATHERING MANAGEMENT
    // ============================================================================

    /**
     * Initialize gathering automation system on server startup
     */
    void Initialize();

    /**
     * Update gathering automation for player (called periodically)
     */
    void Update(::Player* player, uint32 diff);

    /**
     * Enable/disable gathering automation for player
     */
    void SetEnabled(::Player* player, bool enabled);
    bool IsEnabled(::Player* player) const;

    /**
     * Get automation profile for player
     */
    void SetAutomationProfile(uint32 playerGuid, GatheringAutomationProfile const& profile);
    GatheringAutomationProfile GetAutomationProfile(uint32 playerGuid) const;

    // ============================================================================
    // NODE DETECTION
    // ============================================================================

    /**
     * Scan for nearby gathering nodes based on player's professions
     * Returns list of detected nodes sorted by distance
     */
    std::vector<GatheringNodeInfo> ScanForNodes(::Player* player, float range = 40.0f);

    /**
     * Find nearest gathering node for specific profession
     */
    GatheringNodeInfo const* FindNearestNode(::Player* player, ProfessionType profession);

    /**
     * Check if player can gather from node (skill + profession requirements)
     */
    bool CanGatherFromNode(::Player* player, GatheringNodeInfo const& node) const;

    /**
     * Get required skill level for node
     */
    uint16 GetRequiredSkillForNode(GatheringNodeInfo const& node) const;

    /**
     * Determine profession required for node
     */
    ProfessionType GetProfessionForNode(GatheringNodeInfo const& node) const;

    // ============================================================================
    // GATHERING ACTIONS
    // ============================================================================

    /**
     * Attempt to gather from node (pathfind, cast spell, loot)
     * Returns true if gathering was initiated successfully
     */
    bool GatherFromNode(::Player* player, GatheringNodeInfo const& node);

    /**
     * Cast gathering spell on node
     */
    bool CastGatheringSpell(::Player* player, GatheringNodeInfo const& node);

    /**
     * Loot gathered items from node
     */
    bool LootNode(::Player* player, ::GameObject* node);

    /**
     * Skin creature corpse
     */
    bool SkinCreature(::Player* player, ::Creature* creature);

    /**
     * Get gathering spell ID for profession
     */
    uint32 GetGatheringSpellId(ProfessionType profession, uint16 skillLevel) const;

    // ============================================================================
    // PATHFINDING INTEGRATION
    // ============================================================================

    /**
     * Move bot to gathering node
     */
    bool PathToNode(::Player* player, GatheringNodeInfo const& node);

    /**
     * Check if bot is in range to gather from node
     */
    bool IsInGatheringRange(::Player* player, GatheringNodeInfo const& node) const;

    /**
     * Calculate travel distance to node
     */
    float GetDistanceToNode(::Player* player, GatheringNodeInfo const& node) const;

    // ============================================================================
    // INVENTORY MANAGEMENT
    // ============================================================================

    /**
     * Check if player has inventory space for gathering
     */
    bool HasInventorySpace(::Player* player, uint32 requiredSlots = 1) const;

    /**
     * Get current number of free bag slots
     */
    uint32 GetFreeBagSlots(::Player* player) const;

    /**
     * Check if gathering materials need to be deposited
     */
    bool ShouldDepositMaterials(::Player* player) const;

    // ============================================================================
    // STATISTICS
    // ============================================================================

    GatheringStatistics const& GetPlayerStatistics(uint32 playerGuid) const;
    GatheringStatistics const& GetGlobalStatistics() const;

    /**
     * Reset statistics for player
     */
    void ResetStatistics(uint32 playerGuid);

private:
    GatheringAutomation();
    ~GatheringAutomation() = default;

    // ============================================================================
    // INITIALIZATION HELPERS
    // ============================================================================

    void LoadGatheringNodes();
    void LoadGatheringSpells();
    void InitializeNodeDatabase();

    // ============================================================================
    // NODE DETECTION HELPERS
    // ============================================================================

    /**
     * Detect mining veins in range
     */
    std::vector<GatheringNodeInfo> DetectMiningNodes(::Player* player, float range);

    /**
     * Detect herb nodes in range
     */
    std::vector<GatheringNodeInfo> DetectHerbNodes(::Player* player, float range);

    /**
     * Detect fishing pools in range
     */
    std::vector<GatheringNodeInfo> DetectFishingPools(::Player* player, float range);

    /**
     * Detect skinnable creatures in range
     */
    std::vector<GatheringNodeInfo> DetectSkinnableCreatures(::Player* player, float range);

    /**
     * Convert GameObject to GatheringNodeInfo
     */
    GatheringNodeInfo CreateNodeInfo(::GameObject* gameObject, ProfessionType profession);

    /**
     * Convert Creature to GatheringNodeInfo
     */
    GatheringNodeInfo CreateNodeInfo(::Creature* creature);

    // ============================================================================
    // GATHERING HELPERS
    // ============================================================================

    /**
     * Validate node is still active and harvestable
     */
    bool ValidateNode(::Player* player, GatheringNodeInfo const& node);

    /**
     * Calculate skill-up chance for node
     */
    float GetSkillUpChance(::Player* player, GatheringNodeInfo const& node) const;

    /**
     * Handle gathering result (success/failure, loot, skill-up)
     */
    void HandleGatheringResult(::Player* player, GatheringNodeInfo const& node, bool success);

    // ============================================================================
    // DATA STRUCTURES
    // ============================================================================

    // Active gathering nodes cache (playerGuid -> detected nodes)
    std::unordered_map<uint32, std::vector<GatheringNodeInfo>> _detectedNodes;

    // Current gathering target (playerGuid -> node)
    std::unordered_map<uint32, GatheringNodeInfo> _currentTarget;

    // Automation profiles (playerGuid -> profile)
    std::unordered_map<uint32, GatheringAutomationProfile> _profiles;

    // Gathering spell database (profession + skill range -> spell ID)
    struct GatheringSpellInfo
    {
        uint32 spellId;
        uint16 minSkill;
        uint16 maxSkill;
        ProfessionType profession;
    };
    std::vector<GatheringSpellInfo> _gatheringSpells;

    // Node type database (gameObjectEntry -> profession + required skill)
    struct NodeTypeInfo
    {
        ProfessionType profession;
        uint16 requiredSkill;
        GatheringNodeType nodeType;
    };
    std::unordered_map<uint32, NodeTypeInfo> _nodeTypes;

    // Statistics
    std::unordered_map<uint32, GatheringStatistics> _playerStatistics;
    GatheringStatistics _globalStatistics;

    mutable std::mutex _mutex;

    // Update intervals
    static constexpr uint32 NODE_SCAN_INTERVAL = 3000;          // 3 seconds
    static constexpr uint32 GATHERING_CAST_TIME = 2000;         // 2 seconds
    static constexpr uint32 NODE_CACHE_DURATION = 30000;        // 30 seconds
    static constexpr float GATHERING_RANGE = 5.0f;              // Yards

    std::unordered_map<uint32, uint32> _lastScanTimes;
};

} // namespace Playerbot
