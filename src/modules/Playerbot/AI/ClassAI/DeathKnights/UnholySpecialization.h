/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "DeathKnightSpecialization.h"
#include "ObjectGuid.h"
#include <map>
#include <vector>

namespace Playerbot
{

class TC_GAME_API UnholySpecialization : public DeathKnightSpecialization
{
public:
    explicit UnholySpecialization(Player* bot);

    // Import common spells from base class
    using DeathKnightSpecialization::CommonSpells;

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

    // Rune management
    void UpdateRuneManagement() override;
    bool HasAvailableRunes(RuneType type, uint32 count = 1) override;
    void ConsumeRunes(RuneType type, uint32 count = 1) override;
    uint32 GetAvailableRunes(RuneType type) const override;

    // Runic Power management
    void UpdateRunicPowerManagement() override;
    void GenerateRunicPower(uint32 amount) override;
    void SpendRunicPower(uint32 amount) override;
    uint32 GetRunicPower() const override;
    bool HasEnoughRunicPower(uint32 required) const override;

    // Disease management
    void UpdateDiseaseManagement() override;
    void ApplyDisease(::Unit* target, DiseaseType type, uint32 spellId) override;
    bool HasDisease(::Unit* target, DiseaseType type) const override;
    bool ShouldApplyDisease(::Unit* target, DiseaseType type) const override;
    void RefreshExpringDiseases() override;
    bool ShouldRefreshDiseases(::Unit* target) const;

    // Death and Decay management
    void UpdateDeathAndDecay() override;
    bool ShouldCastDeathAndDecay() const override;
    void CastDeathAndDecay(Position targetPos) override;

    // Specialization info
    DeathKnightSpec GetSpecialization() const override { return DeathKnightSpec::UNHOLY; }
    const char* GetSpecializationName() const override { return "Unholy"; }

private:
    // Unholy-specific mechanics
    void UpdateUnholyRotation();
    void UpdateGhoulManagement();
    void UpdateSuddenDoomProcs();
    bool ShouldCastScourgeStrike(::Unit* target);
    bool ShouldCastDeathCoil(::Unit* target);
    bool ShouldCastBoneArmor();
    bool ShouldCastCorpseExplosion();
    bool ShouldCastSummonGargoyle();
    bool ShouldCastArmyOfTheDead();
    bool ShouldCastAntiMagicShell();
    bool ShouldCastDarkTransformation();
    bool ShouldCastDeathPact();
    bool HandleFallbackRotation(::Unit* target);

    // Combat management helpers
    bool HandleEmergencySurvival();
    bool HandleMinionManagement();
    bool HandleUtilitySpells(::Unit* target);
    bool HandleDefensiveCooldowns();
    void UpdateCombatPhase();
    void UpdateTargetPrioritization();
    void UpdateThreatManagement();

    // Rotation execution methods
    bool ExecuteOpeningRotation(::Unit* target);
    bool ExecuteDiseaseApplicationRotation(::Unit* target);
    bool ExecuteBurstRotation(::Unit* target);
    bool ExecuteSustainRotation(::Unit* target);
    bool ExecuteAoeRotation(::Unit* target);
    bool ExecuteExecuteRotation(::Unit* target);

    // Pet management
    void UpdatePetManagement();
    void SummonGhoul();
    void CommandGhoul(::Unit* target);
    void CommandGhoulIfNeeded();
    bool HasActiveGhoul();
    void ManageGhoulHealth();

    // Proc management
    void UpdateProcManagement();
    bool HasSuddenDoomProc();
    void ConsumeSuddenDoomProc();

    // Disease spreading
    void UpdateDiseaseSpread();
    void SpreadDiseases(::Unit* target);
    bool ShouldSpreadDiseases();
    std::vector<::Unit*> GetDiseaseTargets();

    // Unholy abilities
    void CastScourgeStrike(::Unit* target);
    void CastDeathCoil(::Unit* target);
    void CastBoneArmor();
    void CastPlagueStrike(::Unit* target);
    void CastIcyTouch(::Unit* target);
    void CastPestilence(::Unit* target);
    void CastCorpseExplosion();
    void CastDarkTransformation();
    void CastBloodStrike(::Unit* target);
    void CastBloodBoil(::Unit* target);
    void CastNecroticStrike(::Unit* target);
    void CastAntiMagicShell();
    void CastDeathPact();
    void CastDeathStrike(::Unit* target);
    void CastMindFreeze(::Unit* target);
    void CastDeathGrip(::Unit* target);
    void UpdateWeaponEnchantments();

    // Summoning abilities
    void CastSummonGargoyle();
    void CastArmyOfTheDead();
    void CastRaiseAlly(::Unit* target);

    // Unholy presence management
    void EnterUnholyPresence();
    bool ShouldUseUnholyPresence();

    // Corpse management
    void UpdateCorpseManagement();
    bool HasAvailableCorpse();
    Position GetNearestCorpsePosition();

    // Utility helpers
    uint32 GetSpellCooldown(uint32 spellId) const;
    void UpdateRunicPowerDecay(uint32 diff);
    uint32 GetDiseaseRemainingTime(::Unit* target, DiseaseType type) const;
    std::vector<DiseaseInfo> GetActiveDiseases(::Unit* target) const;

    // Unholy rotation phases
    enum UnholyRotationPhase : uint8
    {
        OPENING = 0,
        DISEASE_APPLICATION = 1,
        BURST_PHASE = 2,
        SUSTAIN_PHASE = 3,
        AOE_PHASE = 4,
        EXECUTE_PHASE = 5
    };

    // Disease application phases
    enum DiseasePhase : uint8
    {
        BLOOD_PLAGUE_FIRST = 0,
        FROST_FEVER_SECOND = 1,
        DISEASES_APPLIED = 2
    };

    // Unholy spell IDs
    enum UnholySpells
    {
        SCOURGE_STRIKE = 55090,
        CORPSE_EXPLOSION = 49158,
        DARK_TRANSFORMATION = 63560,
        SUDDEN_DOOM = 49530,
        SUMMON_GARGOYLE = 49206,
        ARMY_OF_THE_DEAD = 42650,
        RAISE_ALLY = 61999,
        GHOUL_FRENZY = 63560,
        NECROTIC_STRIKE = 73975,
        FESTERING_WOUND = 194310,
        APOCALYPSE = 275699,

        // Disease and basic attacks
        ICY_TOUCH = 45477,
        PLAGUE_STRIKE = 45462,
        PESTILENCE = 50842,
        BLOOD_STRIKE = 45902,
        BLOOD_BOIL = 48721,
        DEATH_STRIKE = 49998,
        DEATH_COIL = 47541,

        // Defensive abilities
        BONE_ARMOR = 195181, // Updated for WoW 11.2
        ANTI_MAGIC_SHELL = 48707,
        DEATH_PACT = 48743,

        // Interrupts and utility
        MIND_FREEZE = 47528,
        DEATH_GRIP = 49576,

        // Presence
        UNHOLY_PRESENCE = 48265
    };

    // Pet tracking
    bool _hasActiveGhoul;
    uint32 _ghoulHealth;
    uint32 _maxGhoulHealth;
    ObjectGuid _ghoulGuid;
    uint32 _lastGhoulCommand;
    uint32 _lastGhoulSummon;
    uint32 _lastGhoulHealing;
    uint32 _ghoulCommandCooldown;

    // Proc tracking
    bool _suddenDoomActive;
    uint32 _suddenDoomExpires;
    uint32 _suddenDoomStacks;
    bool _necroticStrikeActive;
    uint32 _necroticStrikeExpires;
    bool _unholyFrenzyActive;
    uint32 _unholyFrenzyExpires;
    bool _corpseExploderActive;
    uint32 _corpseExploderExpires;
    uint32 _lastProcCheck;

    // Summoning cooldowns
    uint32 _summonGargoyleReady;
    uint32 _armyOfTheDeadReady;
    uint32 _darkTransformationReady;
    uint32 _boneArmorReady;
    uint32 _antiMagicShellReady;
    uint32 _deathPactReady;
    uint32 _lastSummonGargoyle;
    uint32 _lastArmyOfTheDead;
    uint32 _lastDarkTransformation;
    uint32 _lastBoneArmor;
    uint32 _lastAntiMagicShell;
    uint32 _lastDeathPact;

    // Disease spread tracking
    uint32 _lastDiseaseSpread;
    std::vector<::Unit*> _diseaseTargets;

    // Corpse tracking
    std::vector<Position> _availableCorpses;
    uint32 _lastCorpseUpdate;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Rotation tracking
    uint32 _lastRotationCheck;
    UnholyRotationPhase _rotationPhase;
    uint32 _combatStartTime;

    // Threat and emergency tracking
    uint32 _lastThreatCheck;
    uint32 _threatReduction;
    bool _emergencyHealUsed;
    uint32 _lastEmergencyHeal;

    // AOE and target tracking
    uint32 _aoeTargetCount;
    uint32 _lastAoeCheck;
    ObjectGuid _priorityTarget;
    uint32 _lastTargetSwitch;

    // Disease application tracking
    DiseasePhase _diseaseApplicationPhase;
    uint32 _lastDiseaseCheck;

    // Combat state tracking
    bool _multiTargetMode;
    bool _executePhase;
    bool _burstPhase;
    uint32 _lastBurstCheck;
    bool _conserveMode;
    uint32 _lastResourceCheck;

    // Performance tracking
    uint32 _totalDamageDealt;
    uint32 _diseaseDamage;
    uint32 _procActivations;
    uint32 _runicPowerSpent;
    uint32 _runicPowerGenerated;
    uint32 _scourgeStrikesUsed;
    uint32 _deathCoilsUsed;
    uint32 _pestilenceCount;
    uint32 _minionsActive;

    // Constants
    static constexpr float UNHOLY_MELEE_RANGE = 5.0f;
    static constexpr uint32 SUDDEN_DOOM_DURATION = 10000; // 10 seconds
    static constexpr uint32 SUMMON_GARGOYLE_COOLDOWN = 180000; // 3 minutes
    static constexpr uint32 ARMY_OF_THE_DEAD_COOLDOWN = 600000; // 10 minutes
    static constexpr uint32 DARK_TRANSFORMATION_COOLDOWN = 120000; // 2 minutes
    static constexpr uint32 GHOUL_COMMAND_INTERVAL = 3000; // 3 seconds
    static constexpr uint32 DISEASE_SPREAD_INTERVAL = 5000; // 5 seconds
    static constexpr uint32 CORPSE_UPDATE_INTERVAL = 10000; // 10 seconds
    static constexpr uint32 PROC_CHECK_INTERVAL = 500; // 0.5 seconds
    static constexpr float BONE_ARMOR_THRESHOLD = 0.5f; // 50%
    static constexpr uint32 BONE_ARMOR_MAX_STACKS = 5;
    static constexpr float DISEASE_SPREAD_RANGE = 10.0f;
    // Additional cooldowns
    static constexpr uint32 BONE_ARMOR_COOLDOWN = 30000; // 30 seconds
    static constexpr uint32 ANTI_MAGIC_SHELL_COOLDOWN = 45000; // 45 seconds
    static constexpr uint32 DEATH_PACT_COOLDOWN = 120000; // 2 minutes
    // Update intervals and ranges
    static constexpr uint32 ROTATION_UPDATE_INTERVAL = 100; // 0.1 seconds
    static constexpr uint32 TARGET_SWITCH_INTERVAL = 2000; // 2 seconds
    static constexpr uint32 THREAT_CHECK_INTERVAL = 3000; // 3 seconds
    static constexpr float AOE_CHECK_RANGE = 15.0f; // 15 yards
    static constexpr uint32 GHOUL_HEAL_INTERVAL = 5000; // 5 seconds
    static constexpr uint32 NECROTIC_STRIKE_DURATION = 15000; // 15 seconds
    static constexpr uint32 UNHOLY_FRENZY_DURATION = 12000; // 12 seconds
};

} // namespace Playerbot