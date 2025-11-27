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
#include <string>
#include <vector>
#include <cstdint>
#include <atomic>

// Forward declarations
class Player;

namespace Playerbot
{

// Forward declarations
struct BattlePetInfo;
struct PetTeam;
struct PetBattleAutomationProfile;
enum class PetQuality : uint8;
enum class PetFamily : uint8;

/**
 * @brief Battle pet metrics (moved to namespace scope to avoid conflicts)
 */
struct PetMetrics
{
    std::atomic<uint32> petsCollected{0};
    std::atomic<uint32> battlesWon{0};
    std::atomic<uint32> battlesLost{0};
    std::atomic<uint32> raresCaptured{0};
    std::atomic<uint32> petsLeveled{0};
    std::atomic<uint32> totalXPGained{0};

    // Additional battle statistics
    std::atomic<uint32> battlesStarted{0};     // Total battles started
    std::atomic<uint32> battlesForfeited{0};   // Battles forfeited
    std::atomic<uint32> petsSwitched{0};       // Times pet was switched during battle
    std::atomic<uint32> abilitiesUsed{0};      // Total abilities used in battles
    std::atomic<uint32> damageDealt{0};        // Total damage dealt in battles
    std::atomic<uint32> healingDone{0};        // Total healing done in battles
    std::atomic<uint32> raresFound{0};         // Rare pets discovered (not captured)
    std::atomic<uint32> criticalHits{0};       // Critical hits landed
    std::atomic<uint32> dodges{0};             // Attacks dodged

    void Reset()
    {
        petsCollected = 0;
        battlesWon = 0;
        battlesLost = 0;
        raresCaptured = 0;
        petsLeveled = 0;
        totalXPGained = 0;
        battlesStarted = 0;
        battlesForfeited = 0;
        petsSwitched = 0;
        abilitiesUsed = 0;
        damageDealt = 0;
        healingDone = 0;
        raresFound = 0;
        criticalHits = 0;
        dodges = 0;
    }

    float GetWinRate() const
    {
        uint32 total = battlesWon + battlesLost;
        return total > 0 ? (float)battlesWon / total : 0.0f;
    }

    float GetForfeitRate() const
    {
        uint32 total = battlesStarted.load();
        return total > 0 ? (float)battlesForfeited / total : 0.0f;
    }

    float GetAverageAbilitiesPerBattle() const
    {
        uint32 total = battlesStarted.load();
        return total > 0 ? (float)abilitiesUsed / total : 0.0f;
    }

    float GetDamagePerBattle() const
    {
        uint32 total = battlesWon + battlesLost;
        return total > 0 ? (float)damageDealt / total : 0.0f;
    }
};

/**
 * @brief Interface for battle pet automation system
 *
 * Provides complete battle pet management including collection, battle AI,
 * leveling automation, team composition, and rare pet tracking.
 */
class TC_GAME_API IBattlePetManager
{
public:
    virtual ~IBattlePetManager() = default;

    // Core pet management
    virtual void Initialize() = 0;
    virtual void Update(uint32 diff) = 0;
    virtual std::vector<BattlePetInfo> GetPlayerPets() const = 0;
    virtual bool OwnsPet(uint32 speciesId) const = 0;
    virtual bool CapturePet(uint32 speciesId, PetQuality quality) = 0;
    virtual bool ReleasePet(uint32 speciesId) = 0;
    virtual uint32 GetPetCount() const = 0;

    // Pet battle AI
    virtual bool StartPetBattle(uint32 targetNpcId) = 0;
    virtual bool ExecuteBattleTurn() = 0;
    virtual uint32 SelectBestAbility() const = 0;
    virtual bool SwitchActivePet(uint32 petIndex) = 0;
    virtual bool UseAbility(uint32 abilityId) = 0;
    virtual bool ShouldCapturePet() const = 0;
    virtual bool ForfeitBattle() = 0;

    // Pet leveling
    virtual void AutoLevelPets() = 0;
    virtual std::vector<BattlePetInfo> GetPetsNeedingLevel() const = 0;
    virtual uint32 GetXPRequiredForLevel(uint32 currentLevel) const = 0;
    virtual void AwardPetXP(uint32 speciesId, uint32 xp) = 0;
    virtual bool LevelUpPet(uint32 speciesId) = 0;

    // Team composition
    virtual bool CreatePetTeam(std::string const& teamName, std::vector<uint32> const& petSpeciesIds) = 0;
    virtual std::vector<PetTeam> GetPlayerTeams() const = 0;
    virtual bool SetActiveTeam(std::string const& teamName) = 0;
    virtual PetTeam GetActiveTeam() const = 0;
    virtual std::vector<uint32> OptimizeTeamForOpponent(PetFamily opponentFamily) const = 0;

    // Pet healing
    virtual bool HealAllPets() = 0;
    virtual bool HealPet(uint32 speciesId) = 0;
    virtual bool NeedsHealing(uint32 speciesId) const = 0;
    virtual uint32 FindNearestPetHealer() const = 0;

    // Rare pet tracking
    virtual void TrackRarePetSpawns() = 0;
    virtual bool IsRarePet(uint32 speciesId) const = 0;
    virtual std::vector<uint32> GetRarePetsInZone() const = 0;
    virtual bool NavigateToRarePet(uint32 speciesId) = 0;

    // Automation profiles
    virtual void SetAutomationProfile(PetBattleAutomationProfile const& profile) = 0;
    virtual PetBattleAutomationProfile GetAutomationProfile() const = 0;

    // Metrics
    virtual PetMetrics const& GetMetrics() const = 0;
    virtual PetMetrics const& GetGlobalMetrics() const = 0;
};

} // namespace Playerbot
