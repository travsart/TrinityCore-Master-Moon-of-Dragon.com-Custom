/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "WarriorSpecialization.h"
#include <map>
#include <queue>

namespace Playerbot
{

// Threat priority levels
enum class ThreatPriority : uint8
{
    CRITICAL = 0,    // Immediate threat response needed
    HIGH = 1,        // High priority threat target
    MODERATE = 2,    // Normal threat management
    LOW = 3,         // Low priority or controlled
    NONE = 4         // No threat issues
};

// Threat target info
struct ThreatTarget
{
    ::Unit* target;
    ThreatPriority priority;
    float threatPercent;
    bool attacking;
    uint32 timestamp;

    ThreatTarget() : target(nullptr), priority(ThreatPriority::NONE), threatPercent(0.0f), attacking(false), timestamp(0) {}

    ThreatTarget(::Unit* t, ThreatPriority p, float threat)
        : target(t), priority(p), threatPercent(threat), attacking(t ? t->GetVictim() != nullptr : false), timestamp(getMSTime()) {}

    bool operator<(const ThreatTarget& other) const
    {
        if (priority != other.priority)
            return priority > other.priority;
        return threatPercent < other.threatPercent;
    }
};

class TC_GAME_API ProtectionSpecialization : public WarriorSpecialization
{
public:
    explicit ProtectionSpecialization(Player* bot);

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

    // Stance management
    void UpdateStance() override;
    WarriorStance GetOptimalStance(::Unit* target) override;
    void SwitchStance(WarriorStance stance) override;

    // Specialization info
    WarriorSpec GetSpecialization() const override { return WarriorSpec::PROTECTION; }
    const char* GetSpecializationName() const override { return "Protection"; }

private:
    // Protection-specific mechanics
    void UpdateThreatManagement();
    void UpdateShieldMastery();
    void UpdateDefensiveStance();
    void UpdateTaunt();
    bool ShouldCastShieldSlam(::Unit* target);
    bool ShouldCastRevenge(::Unit* target);
    bool ShouldCastDevastateorSunder(::Unit* target);
    bool ShouldCastThunderClap();
    bool ShouldTaunt(::Unit* target);

    // Shield and blocking mechanics
    void OptimizeShieldUsage();
    void UpdateShieldBlock();
    void CastShieldBlock();
    void CastShieldWall();
    bool HasShieldEquipped();
    bool ShouldUseShieldWall();
    uint32 GetShieldBlockCharges();

    // Threat generation and management
    void GenerateThreat();
    void ManageMultipleTargets();
    void UpdateThreatList();
    std::vector<::Unit*> GetThreatTargets();
    ::Unit* GetHighestThreatTarget();
    bool HasThreat(::Unit* target);
    float GetThreatPercent(::Unit* target);

    // Protection rotation spells
    void CastShieldSlam(::Unit* target);
    void CastRevenge(::Unit* target);
    void CastDevastate(::Unit* target);
    void CastSunderArmor(::Unit* target);
    void CastThunderClap();
    void CastConcussionBlow(::Unit* target);
    void CastTaunt(::Unit* target);

    // Defensive cooldowns
    void UpdateDefensiveCooldowns();
    void UseDefensiveCooldowns();
    void CastShieldWall();
    void CastLastStand();
    void CastSpellReflection();
    void CastChallenging();
    bool ShouldUseLastStand();
    bool ShouldUseSpellReflection();

    // Multi-target tanking
    void HandleMultipleEnemies();
    void MaintainThreatOnAll();
    void PickupLooseTargets();
    std::vector<::Unit*> GetUncontrolledEnemies();
    void TauntLooseTarget(::Unit* target);

    // Positioning for tanking
    void OptimizeTankPositioning();
    void FaceAllEnemies();
    void PositionForGroupProtection();
    Position GetOptimalTankPosition();

    // Sunder Armor stacking
    void ManageSunderArmor();
    void ApplySunderArmor(::Unit* target);
    uint32 GetSunderArmorStacks(::Unit* target);
    bool NeedsSunderArmor(::Unit* target);

    // Emergency abilities
    void HandleEmergencies();
    void UseEmergencyAbilities();
    bool IsInDangerousSituation();
    void CallForHelp();

    // Protection spell IDs
    enum ProtectionSpells
    {
        SHIELD_SLAM = 23922,
        REVENGE = 6572,
        DEVASTATE = 20243,
        SUNDER_ARMOR = 7386,
        THUNDER_CLAP = 6343,
        CONCUSSION_BLOW = 12809,
        TAUNT = 355,
        CHALLENGING_SHOUT = 1161,
        SHIELD_BLOCK = 2565,
        SHIELD_WALL = 871,
        LAST_STAND = 12975,
        SPELL_REFLECTION = 23920,
        DISARM = 676,
        SHIELD_BASH = 72
    };

    // State tracking
    uint32 _lastTaunt;
    uint32 _lastShieldBlock;
    uint32 _lastShieldWall;
    uint32 _shieldBlockCharges;
    bool _hasShieldEquipped;

    // Threat tracking per target
    std::map<uint64, float> _threatLevels;
    std::map<uint64, uint32> _sunderArmorStacks;
    std::priority_queue<ThreatTarget> _threatQueue;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance optimization
    uint32 _lastThreatCheck;
    uint32 _lastShieldCheck;
    uint32 _lastPositionCheck;
    uint32 _lastSunderCheck;
    uint32 _lastRotationUpdate;

    // Multi-target tracking
    std::vector<uint64> _controlledTargets;
    std::vector<uint64> _looseTargets;
    uint32 _lastTargetScan;

    // Emergency state
    bool _emergencyMode;
    uint32 _emergencyStartTime;

    // Constants
    static constexpr uint32 SUNDER_ARMOR_DURATION = 30000; // 30 seconds
    static constexpr uint32 MAX_SUNDER_STACKS = 5;
    static constexpr uint32 SHIELD_BLOCK_DURATION = 10000; // 10 seconds
    static constexpr uint32 TAUNT_COOLDOWN = 10000; // 10 seconds
    static constexpr float THREAT_THRESHOLD = 80.0f; // 80% threat to taunt
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 25.0f;
    static constexpr uint32 SHIELD_SLAM_RAGE_COST = 20;
    static constexpr uint32 REVENGE_RAGE_COST = 5;
    static constexpr uint32 DEVASTATE_RAGE_COST = 15;
};

} // namespace Playerbot