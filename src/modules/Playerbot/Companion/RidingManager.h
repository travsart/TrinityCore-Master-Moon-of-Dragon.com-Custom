/*
 * Copyright (C) 2024-2026 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include "Core/DI/Interfaces/IRidingManager.h"
#include "Player.h"
#include "ObjectGuid.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <atomic>

namespace Playerbot
{

// Forward declarations
class MountManager;
class UnifiedMovementCoordinator;

/**
 * @brief Riding Manager - Humanized riding skill and mount acquisition
 *
 * **Phase 6.4: Per-Bot Instance Pattern (29th Manager)**
 *
 * Features:
 * - Automatically detects when bot needs riding skill
 * - Finds appropriate riding trainers for race/faction
 * - Travels to trainers using movement system
 * - Learns riding skills via trainer interaction
 * - Finds and purchases mounts from vendors
 * - Respects gold minimums (won't leave bot broke)
 * - Integrates with MountManager for actual mount usage
 *
 * **Ownership:**
 * - Owned by GameSystemsManager (29th manager)
 * - Each bot has independent riding acquisition state
 * - Static trainer/vendor database shared across all bots
 *
 * **Workflow:**
 * 1. Update() checks if bot needs riding (level-based thresholds)
 * 2. If needed, starts acquisition state machine
 * 3. State machine: Find trainer -> Travel -> Learn -> Find vendor -> Travel -> Buy
 * 4. Once complete, MountManager handles actual mounting
 */
class TC_GAME_API RidingManager final : public IRidingManager
{
public:
    /**
     * @brief Construct riding manager for specific bot
     * @param bot The bot player this manager serves
     */
    explicit RidingManager(Player* bot);
    ~RidingManager();

    // ========================================================================
    // LIFECYCLE (IRidingManager interface)
    // ========================================================================

    void Initialize() override;
    void Update(uint32 diff) override;

    // ========================================================================
    // SKILL CHECKING (IRidingManager interface)
    // ========================================================================

    RidingSkillLevel GetCurrentSkillLevel() const override;
    RidingSkillLevel GetNextSkillLevel() const override;
    bool NeedsRidingSkill() const override;
    bool NeedsMount() const override;
    bool CanAffordNextSkill() const override;
    bool CanAffordMount() const override;

    // ========================================================================
    // TRAINER/VENDOR LOOKUP (IRidingManager interface)
    // ========================================================================

    RidingTrainerInfo const* FindNearestTrainer(RidingSkillLevel skillLevel) const override;
    MountVendorInfo const* FindNearestMountVendor() const override;
    std::vector<RidingTrainerInfo> FindAllTrainers() const override;
    std::vector<MountVendorInfo> FindAllMountVendors() const override;

    // ========================================================================
    // ACQUISITION STATE MACHINE (IRidingManager interface)
    // ========================================================================

    RidingAcquisitionState GetAcquisitionState() const override;
    bool StartRidingAcquisition(RidingSkillLevel skillLevel = RidingSkillLevel::NONE) override;
    bool StartMountAcquisition() override;
    void CancelAcquisition() override;
    bool IsAcquiring() const override;

    // ========================================================================
    // INSTANT LEARNING (IRidingManager interface)
    // ========================================================================

    bool InstantLearnRiding(RidingSkillLevel skillLevel) override;
    bool InstantLearnMount(uint32 mountSpellId) override;

    // ========================================================================
    // CONFIGURATION (IRidingManager interface)
    // ========================================================================

    void SetAutoAcquireEnabled(bool enabled) override;
    bool IsAutoAcquireEnabled() const override;
    void SetMinReserveGold(uint64 goldCopper) override;
    uint64 GetMinReserveGold() const override;

    // ========================================================================
    // METRICS
    // ========================================================================

    struct RidingMetrics
    {
        std::atomic<uint32> skillsLearned{0};
        std::atomic<uint32> mountsPurchased{0};
        std::atomic<uint32> trainerVisits{0};
        std::atomic<uint32> vendorVisits{0};
        std::atomic<uint32> failedAttempts{0};
        std::atomic<uint64> goldSpent{0};  // In copper

        void Reset()
        {
            skillsLearned = 0;
            mountsPurchased = 0;
            trainerVisits = 0;
            vendorVisits = 0;
            failedAttempts = 0;
            goldSpent = 0;
        }
    };

    RidingMetrics const& GetMetrics() const { return _metrics; }
    static RidingMetrics const& GetGlobalMetrics() { return _globalMetrics; }

private:
    // Non-copyable
    RidingManager(RidingManager const&) = delete;
    RidingManager& operator=(RidingManager const&) = delete;

    // ========================================================================
    // DATABASE INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize the static trainer/vendor database (called once)
     */
    static void InitializeDatabase();

    /**
     * @brief Initialize Alliance riding trainers
     */
    static void InitializeAllianceTrainers();

    /**
     * @brief Initialize Horde riding trainers
     */
    static void InitializeHordeTrainers();

    /**
     * @brief Initialize neutral/Pandaren trainers
     */
    static void InitializeNeutralTrainers();

    /**
     * @brief Initialize Alliance mount vendors
     */
    static void InitializeAllianceVendors();

    /**
     * @brief Initialize Horde mount vendors
     */
    static void InitializeHordeVendors();

    // ========================================================================
    // STATE MACHINE HELPERS
    // ========================================================================

    /**
     * @brief Process NEED_RIDING_SKILL state
     */
    void ProcessNeedRidingSkill();

    /**
     * @brief Process TRAVELING_TO_TRAINER state
     */
    void ProcessTravelingToTrainer();

    /**
     * @brief Process AT_TRAINER state
     */
    void ProcessAtTrainer();

    /**
     * @brief Process LEARNING_SKILL state
     */
    void ProcessLearningSkill();

    /**
     * @brief Process NEED_MOUNT state
     */
    void ProcessNeedMount();

    /**
     * @brief Process TRAVELING_TO_VENDOR state
     */
    void ProcessTravelingToVendor();

    /**
     * @brief Process AT_VENDOR state
     */
    void ProcessAtVendor();

    /**
     * @brief Process PURCHASING_MOUNT state
     */
    void ProcessPurchasingMount();

    /**
     * @brief Transition to a new state
     * @param newState The state to transition to
     */
    void TransitionTo(RidingAcquisitionState newState);

    // ========================================================================
    // SKILL HELPERS
    // ========================================================================

    /**
     * @brief Get spell ID for a riding skill level
     * @param skillLevel The skill level
     * @return Spell ID to learn, 0 if invalid
     */
    static uint32 GetSpellIdForSkill(RidingSkillLevel skillLevel);

    /**
     * @brief Get gold cost (in copper) for a skill level
     * @param skillLevel The skill level
     * @return Cost in copper
     */
    static uint64 GetSkillCost(RidingSkillLevel skillLevel);

    /**
     * @brief Get minimum level required for a skill level
     * @param skillLevel The skill level
     * @return Required player level
     */
    static uint32 GetLevelRequirement(RidingSkillLevel skillLevel);

    /**
     * @brief Check if bot already has a specific riding skill
     * @param skillLevel The skill level to check
     * @return true if bot has this skill
     */
    bool HasSkill(RidingSkillLevel skillLevel) const;

    // ========================================================================
    // TRAVEL HELPERS
    // ========================================================================

    /**
     * @brief Start traveling to a position
     * @param mapId Target map ID
     * @param x Target X coordinate
     * @param y Target Y coordinate
     * @param z Target Z coordinate
     * @return true if travel started successfully
     */
    bool StartTravelTo(uint32 mapId, float x, float y, float z);

    /**
     * @brief Check if bot has arrived at destination
     * @return true if within interaction range
     */
    bool HasArrivedAtDestination() const;

    /**
     * @brief Get distance to current target NPC
     * @return Distance in yards
     */
    float GetDistanceToTarget() const;

    // ========================================================================
    // NPC INTERACTION HELPERS
    // ========================================================================

    /**
     * @brief Find the trainer NPC in the world
     * @return Creature pointer or nullptr
     */
    Creature* FindTrainerNPC() const;

    /**
     * @brief Find the vendor NPC in the world
     * @return Creature pointer or nullptr
     */
    Creature* FindVendorNPC() const;

    /**
     * @brief Interact with trainer to learn skill
     * @param trainer The trainer creature
     * @return true if learning succeeded
     */
    bool InteractWithTrainer(Creature* trainer);

    /**
     * @brief Interact with vendor to purchase mount
     * @param vendor The vendor creature
     * @return true if purchase succeeded
     */
    bool InteractWithVendor(Creature* vendor);

    // ========================================================================
    // MOUNT SELECTION HELPERS
    // ========================================================================

    /**
     * @brief Get appropriate mount spell ID for bot's race
     * @param level Bot's level (affects mount tier)
     * @return Mount spell ID
     */
    uint32 GetRaceAppropriateMount(uint32 level) const;

    /**
     * @brief Get mount cost for bot's level tier
     * @param level Bot's level
     * @return Cost in copper
     */
    static uint64 GetMountCost(uint32 level);

    // ========================================================================
    // DATA MEMBERS
    // ========================================================================

    // Bot reference (non-owning)
    Player* _bot;

    // Current acquisition state
    RidingAcquisitionState _state{RidingAcquisitionState::IDLE};

    // Target skill being acquired
    RidingSkillLevel _targetSkill{RidingSkillLevel::NONE};

    // Current target trainer/vendor
    RidingTrainerInfo _targetTrainer;
    MountVendorInfo _targetVendor;

    // Target position for travel
    float _targetX{0.0f};
    float _targetY{0.0f};
    float _targetZ{0.0f};
    uint32 _targetMapId{0};

    // Configuration
    bool _autoAcquireEnabled{true};
    uint64 _minReserveGold{0};  // In copper

    // Timing
    uint32 _updateTimer{0};
    uint32 _stateTimer{0};
    uint32 _interactionTimer{0};

    // Per-bot metrics
    RidingMetrics _metrics;

    // Static shared database
    static std::vector<RidingTrainerInfo> _allianceTrainers;
    static std::vector<RidingTrainerInfo> _hordeTrainers;
    static std::vector<RidingTrainerInfo> _neutralTrainers;
    static std::vector<MountVendorInfo> _allianceVendors;
    static std::vector<MountVendorInfo> _hordeVendors;
    static bool _databaseInitialized;
    static RidingMetrics _globalMetrics;

    // Update intervals
    static constexpr uint32 UPDATE_INTERVAL = 5000;        // 5 seconds between checks
    static constexpr uint32 STATE_TIMEOUT = 120000;        // 2 minutes max per state
    static constexpr uint32 INTERACTION_DELAY = 1500;      // 1.5 seconds for NPC interaction
    static constexpr float INTERACTION_RANGE = 5.0f;       // Must be within 5 yards of NPC
    static constexpr float ARRIVAL_THRESHOLD = 10.0f;      // Consider arrived within 10 yards

    // Riding skill spell IDs (WoW 12.0)
    static constexpr uint32 SPELL_APPRENTICE_RIDING = 33388;
    static constexpr uint32 SPELL_JOURNEYMAN_RIDING = 33391;
    static constexpr uint32 SPELL_EXPERT_RIDING = 34090;
    static constexpr uint32 SPELL_ARTISAN_RIDING = 34091;
    static constexpr uint32 SPELL_MASTER_RIDING = 90265;
    static constexpr uint32 SPELL_COLD_WEATHER_FLYING = 54197;
    static constexpr uint32 SPELL_FLIGHT_MASTERS_LICENSE = 90267;

    // Gold costs in copper (WoW 12.0 - significantly reduced from earlier expansions)
    static constexpr uint64 COST_APPRENTICE = 40000;       // 4 gold
    static constexpr uint64 COST_JOURNEYMAN = 500000;      // 50 gold
    static constexpr uint64 COST_EXPERT = 2500000;         // 250 gold
    static constexpr uint64 COST_ARTISAN = 50000000;       // 5000 gold
    static constexpr uint64 COST_MASTER = 50000000;        // 5000 gold
    static constexpr uint64 COST_MOUNT_BASIC = 10000;      // 1 gold for basic mount
    static constexpr uint64 COST_MOUNT_EPIC = 1000000;     // 100 gold for epic mount
    static constexpr uint64 COST_MOUNT_FLYING = 500000;    // 50 gold for basic flying
};

} // namespace Playerbot
