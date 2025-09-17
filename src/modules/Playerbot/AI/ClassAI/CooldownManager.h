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
#include "Player.h"
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>

namespace Playerbot
{

// Information about a spell's cooldown state
struct CooldownInfo
{
    uint32 spellId;                 // Spell ID
    uint32 cooldownMs;              // Total cooldown duration in milliseconds
    uint32 remainingMs;             // Remaining cooldown time in milliseconds
    bool onGCD;                     // Whether this spell triggers global cooldown
    uint32 charges;                 // Current number of charges (for charge-based spells)
    uint32 maxCharges;              // Maximum number of charges
    uint32 chargeRechargeMs;        // Time to recharge one charge
    uint32 nextChargeTime;          // When the next charge will be available
    bool isChanneling;              // Whether this spell is currently channeling
    uint32 channelDuration;         // Channel duration if channeling

    CooldownInfo() : spellId(0), cooldownMs(0), remainingMs(0), onGCD(true),
                     charges(1), maxCharges(1), chargeRechargeMs(0), nextChargeTime(0),
                     isChanneling(false), channelDuration(0) {}

    CooldownInfo(uint32 spell, uint32 cd, bool gcd = true)
        : spellId(spell), cooldownMs(cd), remainingMs(cd), onGCD(gcd),
          charges(1), maxCharges(1), chargeRechargeMs(cd), nextChargeTime(0),
          isChanneling(false), channelDuration(0) {}

    // Check if spell is ready to use
    bool IsReady() const { return remainingMs == 0 && charges > 0; }

    // Get remaining cooldown as percentage (0.0 = ready, 1.0 = just used)
    float GetCooldownPercent() const
    {
        return cooldownMs > 0 ? static_cast<float>(remainingMs) / cooldownMs : 0.0f;
    }
};

// Manages spell cooldowns and charges for a bot
class TC_GAME_API CooldownManager
{
public:
    CooldownManager();
    ~CooldownManager() = default;

    // Update all cooldowns
    void Update(uint32 diff);

    // Cooldown management
    void StartCooldown(uint32 spellId, uint32 cooldownMs);
    void StartCooldown(uint32 spellId, uint32 cooldownMs, bool triggersGCD);
    void ResetCooldown(uint32 spellId);
    void ReduceCooldown(uint32 spellId, uint32 reductionMs);

    // Cooldown queries
    bool IsReady(uint32 spellId) const;
    uint32 GetRemaining(uint32 spellId) const;
    float GetRemainingPercent(uint32 spellId) const;
    uint32 GetTotalCooldown(uint32 spellId) const;

    // Global cooldown management
    void TriggerGCD(uint32 durationMs = 1500);
    bool IsGCDReady() const;
    uint32 GetGCDRemaining() const;
    void SetGCDDuration(uint32 durationMs) { _gcdDuration = durationMs; }

    // Charge-based abilities
    void SetCharges(uint32 spellId, uint32 current, uint32 maximum, uint32 rechargeTimeMs = 0);
    uint32 GetCharges(uint32 spellId) const;
    uint32 GetMaxCharges(uint32 spellId) const;
    void ConsumeCharge(uint32 spellId);
    void AddCharge(uint32 spellId);
    uint32 GetNextChargeTime(uint32 spellId) const;

    // Channeling spells
    void StartChanneling(uint32 spellId, uint32 channelDurationMs);
    void StopChanneling(uint32 spellId);
    bool IsChanneling(uint32 spellId) const;
    bool IsChannelingAny() const;
    uint32 GetChannelRemaining(uint32 spellId) const;

    // Batch operations
    void ResetAllCooldowns();
    void ReduceAllCooldowns(uint32 reductionMs);
    std::vector<uint32> GetSpellsOnCooldown() const;
    std::vector<uint32> GetReadySpells(const std::vector<uint32>& spellIds) const;

    // Cooldown categories (for talents/effects that affect multiple spells)
    void SetCooldownCategory(uint32 spellId, uint32 categoryId);
    void StartCategoryCooldown(uint32 categoryId, uint32 cooldownMs);
    bool IsCategoryReady(uint32 categoryId) const;

    // Advanced features
    void SetCooldownMultiplier(float multiplier) { _cooldownMultiplier = multiplier; }
    float GetCooldownMultiplier() const { return _cooldownMultiplier; }

    // Prediction - when will spell be ready
    uint32 GetTimeUntilReady(uint32 spellId) const;
    bool WillBeReadyIn(uint32 spellId, uint32 timeMs) const;

    // Statistics
    uint32 GetTotalSpellsTracked() const;
    uint32 GetSpellsOnCooldownCount() const;
    uint32 GetAverageActiveCooldowns() const;

    // Debug and monitoring
    void DumpCooldowns() const;
    CooldownInfo GetCooldownInfo(uint32 spellId) const;

private:
    // Thread-safe cooldown storage
    mutable std::mutex _cooldownMutex;
    std::unordered_map<uint32, CooldownInfo> _cooldowns;

    // Category cooldowns (spell schools, etc.)
    mutable std::mutex _categoryMutex;
    std::unordered_map<uint32, uint32> _spellCategories;     // spellId -> categoryId
    std::unordered_map<uint32, uint32> _categoryCooldowns;   // categoryId -> remainingMs

    // Global cooldown state
    std::atomic<uint32> _globalCooldown{0};
    uint32 _gcdDuration{1500}; // Default 1.5 seconds

    // Cooldown modifiers
    std::atomic<float> _cooldownMultiplier{1.0f};

    // Performance tracking
    mutable std::atomic<uint32> _totalUpdates{0};
    mutable std::atomic<uint32> _activeCooldowns{0};

    // Internal helpers
    void UpdateCooldown(CooldownInfo& cooldown, uint32 diff);
    void UpdateCharges(CooldownInfo& cooldown, uint32 diff);
    uint32 ApplyCooldownMultiplier(uint32 cooldownMs) const;

    // Initialize spell data from DBC if needed
    void EnsureSpellData(uint32 spellId);

    // Constants
    static constexpr uint32 MAX_TRACKED_SPELLS = 1000;
    static constexpr uint32 CLEANUP_INTERVAL_MS = 30000; // 30 seconds
    uint32 _lastCleanup{0};

    void CleanupExpiredCooldowns();
};

// Utility class for cooldown calculations
class TC_GAME_API CooldownCalculator
{
public:
    // Calculate spell cooldown from spell data
    static uint32 CalculateSpellCooldown(uint32 spellId, Player* caster);

    // Calculate GCD for spell
    static uint32 CalculateGCD(uint32 spellId, Player* caster);

    // Check if spell should trigger GCD
    static bool TriggersGCD(uint32 spellId);

    // Calculate charge recharge time
    static uint32 CalculateChargeRechargeTime(uint32 spellId);

    // Get spell charges from spell data
    static uint32 GetSpellCharges(uint32 spellId);

    // Apply haste to cooldown
    static uint32 ApplyHaste(uint32 cooldownMs, float hastePercent);

    // Apply cooldown reduction effects
    static uint32 ApplyCooldownReduction(uint32 cooldownMs, Player* caster, uint32 spellId);

private:
    // Cache for spell cooldown data
    static std::unordered_map<uint32, uint32> _cooldownCache;
    static std::unordered_map<uint32, bool> _gcdCache;
    static std::mutex _cacheMutex;

    static void CacheSpellData(uint32 spellId);
};

} // namespace Playerbot