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
#include <map>
#include <vector>

namespace Playerbot
{

class TC_GAME_API UnholySpecialization : public DeathKnightSpecialization
{
public:
    explicit UnholySpecialization(Player* bot);

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

    // Pet management
    void UpdatePetManagement();
    void SummonGhoul();
    void CommandGhoul(::Unit* target);
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
    void CastCorpseExplosion();
    void CastDarkTransformation();

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

    // Unholy spell IDs
    enum UnholySpells
    {
        SCOURGE_STRIKE = 55090,
        BONE_ARMOR = 49222,
        CORPSE_EXPLOSION = 49158,
        DARK_TRANSFORMATION = 63560,
        SUDDEN_DOOM = 49530,
        SUMMON_GARGOYLE = 49206,
        ARMY_OF_THE_DEAD = 42650,
        RAISE_ALLY = 61999,
        GHOUL_FRENZY = 63560,
        NECROTIC_STRIKE = 73975,
        FESTERING_WOUND = 194310,
        APOCALYPSE = 275699
    };

    // Pet tracking
    bool _hasActiveGhoul;
    uint32 _ghoulHealth;
    uint32 _lastGhoulCommand;
    uint32 _lastGhoulSummon;

    // Proc tracking
    bool _suddenDoomActive;
    uint32 _suddenDoomExpires;
    uint32 _lastProcCheck;

    // Summoning cooldowns
    uint32 _summonGargoyleReady;
    uint32 _armyOfTheDeadReady;
    uint32 _darkTransformationReady;
    uint32 _lastSummonGargoyle;
    uint32 _lastArmyOfTheDead;
    uint32 _lastDarkTransformation;

    // Disease spread tracking
    uint32 _lastDiseaseSpread;
    std::vector<::Unit*> _diseaseTargets;

    // Corpse tracking
    std::vector<Position> _availableCorpses;
    uint32 _lastCorpseUpdate;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance tracking
    uint32 _totalDamageDealt;
    uint32 _diseaseDamage;
    uint32 _procActivations;
    uint32 _runicPowerSpent;

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
};

} // namespace Playerbot