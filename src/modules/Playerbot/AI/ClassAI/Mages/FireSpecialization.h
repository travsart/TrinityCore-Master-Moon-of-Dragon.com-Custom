/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "MageSpecialization.h"
#include <map>
#include <vector>

namespace Playerbot
{

class TC_GAME_API FireSpecialization : public MageSpecialization
{
public:
    explicit FireSpecialization(Player* bot);

    // Core specialization interface
    void UpdateRotation(::Unit* target) override;
    void UpdateBuffs() override;
    void UpdateCooldowns(uint32 diff) override;
    bool CanUseAbility(uint32 spellId) override;

    // Combat callbacks
    void OnCombatStart(::Unit* target) override;
    void OnCombatEnd() override;

    // Resource management
    bool HasEnoughResource(uint32 spellId) override;
    void ConsumeResource(uint32 spellId) override;

    // Positioning
    Position GetOptimalPosition(::Unit* target) override;
    float GetOptimalRange(::Unit* target) override;

    // Specialization info
    MageSpec GetSpecialization() const override { return MageSpec::FIRE; }
    const char* GetSpecializationName() const override { return "Fire"; }

private:
    // Fire-specific mechanics
    void UpdateHotStreak();
    void UpdateHeatingUp();
    void UpdateCombustion();
    bool HasHotStreak();
    bool HasHeatingUp();
    bool HasCombustion();
    bool ShouldCastPyroblast();
    bool ShouldCastFireball();
    bool ShouldCastFireBlast();
    bool ShouldCastPhoenixFlames();
    bool ShouldUseCombustion();

    // Spell casting methods
    void CastFireball();
    void CastPyroblast();
    void CastFireBlast();
    void CastPhoenixFlames();
    void CastScorch();
    void CastFlamestrike();
    void CastDragonsBreath();
    void CastCombustion();
    void CastMirrorImage();

    // DoT management
    void UpdateDoTs();
    void ApplyIgnite(::Unit* target);
    void CastLivingBomb(::Unit* target);
    bool HasIgnite(::Unit* target);
    bool HasLivingBomb(::Unit* target);
    uint32 GetIgniteTimeRemaining(::Unit* target);

    // AoE and multi-target
    void HandleAoERotation();
    std::vector<::Unit*> GetNearbyEnemies(float range = 8.0f);
    bool ShouldUseAoE();
    void CastMeteor();
    void CastBlastWave();

    // Cooldown management
    void UpdateFireCooldowns(uint32 diff);
    void CheckFireBuffs();
    bool HasCriticalMass();
    void UseCooldowns();

    // Hot Streak management
    void ProcessHotStreak();
    void ProcessHeatingUp();
    void CheckForInstantPyroblast();

    // Fire spell IDs
    enum FireSpells
    {
        FIREBALL = 133,
        PYROBLAST = 11366,
        FIRE_BLAST = 2136,
        PHOENIX_FLAMES = 257541,
        SCORCH = 2948,
        FLAMESTRIKE = 2120,
        DRAGONS_BREATH = 31661,
        COMBUSTION = 190319,
        MIRROR_IMAGE = 55342,
        IGNITE = 12846,
        LIVING_BOMB = 44457,
        METEOR = 153561,
        BLAST_WAVE = 157981,
        HOT_STREAK = 48108,
        HEATING_UP = 48107,
        CRITICAL_MASS = 117216
    };

    // State tracking
    bool _hasHotStreak;
    bool _hasHeatingUp;
    uint32 _lastCritTime;
    uint32 _combustionEndTime;
    bool _inCombustion;

    // DoT tracking per target
    std::map<uint64, uint32> _igniteTimers;
    std::map<uint64, uint32> _livingBombTimers;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance optimization
    uint32 _lastDoTCheck;
    uint32 _lastAoECheck;
    uint32 _lastBuffCheck;
    uint32 _lastRotationUpdate;

    // Multi-target tracking
    std::vector<uint64> _nearbyEnemies;
    uint32 _lastEnemyScan;

    // Constants
    static constexpr uint32 HOT_STREAK_DURATION = 10000; // 10 seconds
    static constexpr uint32 HEATING_UP_DURATION = 10000; // 10 seconds
    static constexpr uint32 COMBUSTION_DURATION = 12000; // 12 seconds
    static constexpr uint32 IGNITE_DURATION = 9000; // 9 seconds
    static constexpr uint32 LIVING_BOMB_DURATION = 12000; // 12 seconds
    static constexpr float AOE_THRESHOLD = 3.0f; // Use AoE with 3+ targets
    static constexpr float SCORCH_RANGE = 40.0f;
    static constexpr float FLAMESTRIKE_RANGE = 8.0f;
};

} // namespace Playerbot