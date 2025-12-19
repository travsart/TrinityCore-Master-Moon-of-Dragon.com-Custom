/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#pragma once

#include "Define.h"
#include <vector>

class Player;
class Position;

namespace Playerbot
{

struct MountInfo;
struct MountAutomationProfile;
enum class MountSpeed : uint16;

/**
 * @brief Interface for mount management system
 *
 * Provides comprehensive mount automation including flying, ground,
 * aquatic, dragonriding, multi-passenger support, and collection tracking.
 */
class TC_GAME_API IMountManager
{
public:
    virtual ~IMountManager() = default;

    // Core mount management
    virtual void Initialize() = 0;
    virtual void Update(uint32 diff) = 0;
    virtual bool MountPlayer() = 0;
    virtual bool DismountPlayer() = 0;
    virtual bool IsMounted() const = 0;
    virtual bool ShouldAutoMount(Position const& destination) const = 0;

    // Mount selection
    virtual MountInfo const* GetBestMount() const = 0;
    virtual MountInfo const* GetFlyingMount() const = 0;
    virtual MountInfo const* GetGroundMount() const = 0;
    virtual MountInfo const* GetAquaticMount() const = 0;
    virtual MountInfo const* GetDragonridingMount() const = 0;
    virtual bool CanUseFlyingMount() const = 0;
    virtual bool IsPlayerUnderwater() const = 0;
    virtual bool CanUseDragonriding() const = 0;

    // Mount collection
    virtual std::vector<MountInfo> GetPlayerMounts() const = 0;
    virtual bool KnowsMount(uint32 spellId) const = 0;
    virtual bool LearnMount(uint32 spellId) = 0;
    virtual uint32 GetMountCount() const = 0;
    virtual bool CanUseMount(MountInfo const& mount) const = 0;

    // Riding skill
    virtual uint32 GetRidingSkill() const = 0;
    virtual bool HasRidingSkill() const = 0;
    virtual bool LearnRidingSkill(uint32 skillLevel) = 0;
    virtual MountSpeed GetMaxMountSpeed() const = 0;

    /**
     * @brief Update riding skill and mounts for bot's current level
     *
     * Called during level-up to automatically teach riding skills
     * and provide appropriate mounts based on level thresholds:
     * - Level 10: Apprentice Riding (60% ground) + ground mount
     * - Level 20: Journeyman Riding (100% ground)
     * - Level 30: Expert Riding (150% flying) + flying mount
     * - Level 40: Artisan Riding (280% flying)
     * - Level 80: Master Riding (310% flying)
     *
     * @return true if any new skills/mounts were learned
     */
    virtual bool UpdateRidingForLevel() = 0;

    // Multi-passenger mounts
    virtual bool IsMultiPassengerMount(MountInfo const& mount) const = 0;
    virtual uint32 GetAvailablePassengerSeats() const = 0;
    virtual bool AddPassenger(::Player* passenger) = 0;
    virtual bool RemovePassenger(::Player* passenger) = 0;

    // Automation
    virtual void SetAutomationProfile(MountAutomationProfile const& profile) = 0;
    virtual MountAutomationProfile GetAutomationProfile() const = 0;

    // Database initialization
    virtual void LoadMountDatabase() = 0;
    virtual void InitializeVanillaMounts() = 0;
    virtual void InitializeTBCMounts() = 0;
    virtual void InitializeWrathMounts() = 0;
    virtual void InitializeCataclysmMounts() = 0;
    virtual void InitializePandariaMounts() = 0;
    virtual void InitializeDraenorMounts() = 0;
    virtual void InitializeLegionMounts() = 0;
    virtual void InitializeBfAMounts() = 0;
    virtual void InitializeShadowlandsMounts() = 0;
    virtual void InitializeDragonflightMounts() = 0;
};

} // namespace Playerbot
