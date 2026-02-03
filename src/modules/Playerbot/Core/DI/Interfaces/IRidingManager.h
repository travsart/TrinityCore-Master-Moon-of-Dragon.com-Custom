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
#include <cstdint>
#include <vector>

class Player;
class Creature;
struct Position;

namespace Playerbot
{

/**
 * @brief Riding skill levels matching WoW 12.0
 *
 * These represent the actual skill values, not spell IDs.
 */
enum class RidingSkillLevel : uint32
{
    NONE           = 0,     // No riding skill
    APPRENTICE     = 75,    // 60% ground speed (Level 10+)
    JOURNEYMAN     = 150,   // 100% ground speed (Level 20+)
    EXPERT         = 225,   // 150% flying speed, 60% ground (Level 30+)
    ARTISAN        = 300,   // 280% flying speed, 100% ground (Level 40+)
    MASTER         = 375,   // 310% flying speed (Level 80+)
    COLD_WEATHER   = 400,   // Northrend flying (Level 68+, Wrath content)
    FLIGHT_MASTERS = 450    // Flight Master's License - Cata/MoP areas
};

/**
 * @brief State of riding skill acquisition process
 */
enum class RidingAcquisitionState : uint8
{
    IDLE,                   // Not actively acquiring riding
    NEED_RIDING_SKILL,      // Determined bot needs riding skill
    TRAVELING_TO_TRAINER,   // Bot is moving to trainer
    AT_TRAINER,             // Bot arrived at trainer
    LEARNING_SKILL,         // Bot is in dialogue learning skill
    NEED_MOUNT,             // Need to purchase a mount
    TRAVELING_TO_VENDOR,    // Bot is moving to mount vendor
    AT_VENDOR,              // Bot arrived at mount vendor
    PURCHASING_MOUNT,       // Bot is purchasing mount
    COMPLETE,               // Successfully acquired riding/mount
    FAILED                  // Failed (not enough gold, trainer not found, etc.)
};

/**
 * @brief Information about a riding trainer
 */
struct RidingTrainerInfo
{
    uint32 creatureEntry;       // NPC Entry ID
    uint32 mapId;               // Map where trainer is located
    float x, y, z;              // Position coordinates
    float orientation;          // Facing direction
    uint32 faction;             // Trainer's faction (Alliance=469, Horde=67)
    uint32 race;                // Primary race this trainer serves (0 = all)
    RidingSkillLevel maxSkill;  // Maximum skill level this trainer teaches
    uint32 goldCostCopper;      // Cost in copper to learn riding

    RidingTrainerInfo() : creatureEntry(0), mapId(0), x(0), y(0), z(0), orientation(0),
        faction(0), race(0), maxSkill(RidingSkillLevel::APPRENTICE), goldCostCopper(0) {}
};

/**
 * @brief Information about a mount vendor
 */
struct MountVendorInfo
{
    uint32 creatureEntry;       // NPC Entry ID
    uint32 mapId;               // Map where vendor is located
    float x, y, z;              // Position coordinates
    float orientation;          // Facing direction
    uint32 faction;             // Vendor's faction
    uint32 race;                // Primary race this vendor serves
    uint32 mountSpellId;        // Primary mount spell sold
    uint32 goldCostCopper;      // Mount cost in copper

    MountVendorInfo() : creatureEntry(0), mapId(0), x(0), y(0), z(0), orientation(0),
        faction(0), race(0), mountSpellId(0), goldCostCopper(0) {}
};

/**
 * @brief Interface for riding skill acquisition manager
 *
 * The RidingManager handles the "humanized" process of learning to ride:
 * - Finding appropriate riding trainers for the bot's race/faction
 * - Traveling to trainers using the movement system
 * - Interacting with trainers to learn riding skills
 * - Purchasing mounts from vendors
 *
 * This complements MountManager: RidingManager handles ACQUISITION,
 * MountManager handles USAGE of riding skills and mounts.
 *
 * **Phase 6.4: Per-Bot Instance Pattern (29th Manager)**
 */
class TC_GAME_API IRidingManager
{
public:
    virtual ~IRidingManager() = default;

    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    /**
     * @brief Initialize the riding manager for this bot
     */
    virtual void Initialize() = 0;

    /**
     * @brief Update the riding acquisition state machine
     * @param diff Time delta in milliseconds
     */
    virtual void Update(uint32 diff) = 0;

    // ========================================================================
    // SKILL CHECKING
    // ========================================================================

    /**
     * @brief Get bot's current riding skill level
     * @return Current skill level (NONE if no riding skill)
     */
    virtual RidingSkillLevel GetCurrentSkillLevel() const = 0;

    /**
     * @brief Get the next riding skill level the bot should learn
     * @return Next skill level based on bot's level, or NONE if maxed
     */
    virtual RidingSkillLevel GetNextSkillLevel() const = 0;

    /**
     * @brief Check if bot needs to learn a riding skill
     * @return true if bot is eligible for a higher riding skill
     */
    virtual bool NeedsRidingSkill() const = 0;

    /**
     * @brief Check if bot needs to acquire a mount
     * @return true if bot has riding skill but no mounts
     */
    virtual bool NeedsMount() const = 0;

    /**
     * @brief Check if bot can afford the next riding skill
     * @return true if bot has enough gold
     */
    virtual bool CanAffordNextSkill() const = 0;

    /**
     * @brief Check if bot can afford a basic mount
     * @return true if bot has enough gold
     */
    virtual bool CanAffordMount() const = 0;

    // ========================================================================
    // TRAINER/VENDOR LOOKUP
    // ========================================================================

    /**
     * @brief Find the nearest riding trainer for the bot's race/faction
     * @param skillLevel The skill level to learn (determines which trainers qualify)
     * @return Pointer to trainer info, nullptr if none found
     */
    virtual RidingTrainerInfo const* FindNearestTrainer(RidingSkillLevel skillLevel) const = 0;

    /**
     * @brief Find the nearest mount vendor for the bot's race/faction
     * @return Pointer to vendor info, nullptr if none found
     */
    virtual MountVendorInfo const* FindNearestMountVendor() const = 0;

    /**
     * @brief Find all riding trainers for the bot's race/faction
     * @return Vector of trainer info structures
     */
    virtual std::vector<RidingTrainerInfo> FindAllTrainers() const = 0;

    /**
     * @brief Find all mount vendors for the bot's race/faction
     * @return Vector of vendor info structures
     */
    virtual std::vector<MountVendorInfo> FindAllMountVendors() const = 0;

    // ========================================================================
    // ACQUISITION STATE MACHINE
    // ========================================================================

    /**
     * @brief Get current state of the acquisition process
     * @return Current state enum value
     */
    virtual RidingAcquisitionState GetAcquisitionState() const = 0;

    /**
     * @brief Start the process of acquiring riding skill
     * @param skillLevel Skill level to acquire (or NONE for auto-detect)
     * @return true if acquisition process started successfully
     */
    virtual bool StartRidingAcquisition(RidingSkillLevel skillLevel = RidingSkillLevel::NONE) = 0;

    /**
     * @brief Start the process of acquiring a mount
     * @return true if acquisition process started successfully
     */
    virtual bool StartMountAcquisition() = 0;

    /**
     * @brief Cancel any ongoing acquisition process
     */
    virtual void CancelAcquisition() = 0;

    /**
     * @brief Check if an acquisition is currently in progress
     * @return true if actively acquiring riding/mount
     */
    virtual bool IsAcquiring() const = 0;

    // ========================================================================
    // INSTANT LEARNING (Debug/GM commands)
    // ========================================================================

    /**
     * @brief Instantly learn riding skill (bypasses travel/gold)
     * @param skillLevel Skill level to learn
     * @return true if skill was learned
     *
     * NOTE: This is for debug/GM use. For humanized behavior,
     * use StartRidingAcquisition() instead.
     */
    virtual bool InstantLearnRiding(RidingSkillLevel skillLevel) = 0;

    /**
     * @brief Instantly grant a mount (bypasses travel/gold)
     * @param mountSpellId Mount spell ID to learn
     * @return true if mount was learned
     *
     * NOTE: This is for debug/GM use. For humanized behavior,
     * use StartMountAcquisition() instead.
     */
    virtual bool InstantLearnMount(uint32 mountSpellId) = 0;

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * @brief Enable or disable automatic riding acquisition
     * @param enabled If true, bot will automatically learn riding when eligible
     */
    virtual void SetAutoAcquireEnabled(bool enabled) = 0;

    /**
     * @brief Check if automatic riding acquisition is enabled
     * @return true if auto-acquire is enabled
     */
    virtual bool IsAutoAcquireEnabled() const = 0;

    /**
     * @brief Set minimum gold to keep after purchasing
     * @param goldCopper Minimum gold in copper to reserve
     */
    virtual void SetMinReserveGold(uint64 goldCopper) = 0;

    /**
     * @brief Get minimum gold reserve setting
     * @return Minimum gold in copper
     */
    virtual uint64 GetMinReserveGold() const = 0;
};

} // namespace Playerbot
