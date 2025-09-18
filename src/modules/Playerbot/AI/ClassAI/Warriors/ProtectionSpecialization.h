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
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <mutex>

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

    // Enhanced state tracking
    std::atomic<uint32> _lastTaunt{0};
    std::atomic<uint32> _lastShieldBlock{0};
    std::atomic<uint32> _lastShieldWall{0};
    std::atomic<uint32> _shieldBlockCharges{0};
    std::atomic<bool> _hasShieldEquipped{false};
    std::atomic<bool> _revengeReady{false};
    std::atomic<bool> _shieldSlamReady{false};
    std::atomic<uint32> _devastateStacks{0};
    std::atomic<bool> _inDefensiveStance{true};

    // Performance metrics
    struct ProtectionMetrics {
        std::atomic<uint32> totalTaunts{0};
        std::atomic<uint32> shieldSlamsLanded{0};
        std::atomic<uint32> revengeProcs{0};
        std::atomic<uint32> threatEvents{0};
        std::atomic<uint32> emergencyActivations{0};
        std::atomic<float> averageThreatGeneration{0.0f};
        std::atomic<float> damageReduction{0.0f};
        std::atomic<float> shieldBlockUptime{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalTaunts = 0; shieldSlamsLanded = 0; revengeProcs = 0;
            threatEvents = 0; emergencyActivations = 0;
            averageThreatGeneration = 0.0f; damageReduction = 0.0f; shieldBlockUptime = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
        }
    } _protectionMetrics;

    // Threat tracking per target
    std::unordered_map<uint64, float> _threatLevels;
    std::unordered_map<uint64, uint32> _sunderArmorStacks;
    std::priority_queue<ThreatTarget> _threatQueue;
    mutable std::shared_mutex _threatMutex;

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
    std::atomic<uint32> _lastTargetScan{0};

    // Emergency state
    std::atomic<bool> _emergencyMode{false};
    std::atomic<uint32> _emergencyStartTime{0};

    // Threat prediction system
    struct ThreatPredictor {
        std::unordered_map<uint64, std::queue<float>> threatHistory;
        void RecordThreat(uint64 targetGuid, float threat) {
            auto& history = threatHistory[targetGuid];
            history.push(threat);
            if (history.size() > 10) // Keep last 10 readings
                history.pop();
        }
        float PredictThreatLoss(uint64 targetGuid) const {
            auto it = threatHistory.find(targetGuid);
            if (it == threatHistory.end() || it->second.size() < 3)
                return 0.0f;
            // Simple trend analysis
            const auto& history = it->second;
            auto temp = history;
            float sum = 0.0f;
            int count = 0;
            while (!temp.empty() && count < 3) {
                sum += temp.back();
                temp.pop();
                count++;
            }
            return sum / count;
        }
    } _threatPredictor;

    // Advanced Protection mechanics
    void OptimizeShieldBlock();
    void ManageThreatRotation(::Unit* target);
    void OptimizeDefensiveCooldowns();
    void HandleTankSwap();
    void OptimizePositionForTanking(::Unit* target);
    void ExecuteEmergencyProtocol();
    void HandleMultiTargetThreat();
    void OptimizeRevengeTiming();
    void ManageShieldMasteryStacks();
    void OptimizeThreatGeneration(::Unit* target);
    void HandleCriticalThreat(::Unit* target);
    void OptimizeSunderArmorApplication();
    void ManageDefensiveStanceOptimally();
    void HandleGroupProtection();
    void OptimizeTauntTiming();
    void ManageThreatCapRotation();
    void HandleSpellReflectionTiming();
    void OptimizeShieldSlamTiming(::Unit* target);
    void ManageAggroRadius();
    float CalculateThreatPerSecond();
    void PredictThreatLoss(::Unit* target);
    void OptimizeResourceForTanking();
    void HandleTankingRotationPriority(::Unit* target);
    void ManageCrowdControlBreaking();
    void OptimizeInterruptRotation(::Unit* target);
    void HandleBossSpecificMechanics(::Unit* target);

    // Enhanced constants
    static constexpr uint32 SUNDER_ARMOR_DURATION = 30000; // 30 seconds
    static constexpr uint32 MAX_SUNDER_STACKS = 5;
    static constexpr uint32 SHIELD_BLOCK_DURATION = 10000; // 10 seconds
    static constexpr uint32 TAUNT_COOLDOWN = 10000; // 10 seconds
    static constexpr float THREAT_THRESHOLD = 80.0f; // 80% threat to taunt
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 25.0f;
    static constexpr uint32 SHIELD_SLAM_RAGE_COST = 20;
    static constexpr uint32 REVENGE_RAGE_COST = 5;
    static constexpr uint32 DEVASTATE_RAGE_COST = 15;
    static constexpr uint32 THUNDER_CLAP_RAGE_COST = 20;
    static constexpr uint32 CONCUSSION_BLOW_RAGE_COST = 25;
    static constexpr float OPTIMAL_THREAT_LEAD = 130.0f; // 30% threat lead
    static constexpr float CRITICAL_THREAT_THRESHOLD = 90.0f;
    static constexpr uint32 SHIELD_BLOCK_CHARGES = 2;
    static constexpr uint32 MAX_TARGETS_FOR_AOE = 3;
    static constexpr float TANK_POSITION_TOLERANCE = 2.0f;
    static constexpr uint32 THREAT_UPDATE_INTERVAL = 500; // 0.5 seconds
    static constexpr float SHIELD_SLAM_THREAT_MULTIPLIER = 1.75f;
    static constexpr float REVENGE_THREAT_MULTIPLIER = 1.5f;
    static constexpr float DEVASTATE_THREAT_MULTIPLIER = 1.0f;
    static constexpr uint32 EMERGENCY_COOLDOWN_THRESHOLD = 3000; // 3 seconds
    static constexpr float BOSS_THREAT_MULTIPLIER = 1.3f;
    static constexpr uint32 SPELL_REFLECT_WINDOW = 2000; // 2 seconds
    static constexpr float MULTI_TARGET_THREAT_THRESHOLD = 50.0f;
    static constexpr uint32 TAUNT_MISS_RETRY_DELAY = 1500; // 1.5 seconds
};

} // namespace Playerbot