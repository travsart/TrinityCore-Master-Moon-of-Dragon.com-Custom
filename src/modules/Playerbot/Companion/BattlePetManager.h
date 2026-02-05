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
#include "Threading/LockHierarchy.h"
#include "Player.h"
#include "ObjectGuid.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace Playerbot
{


/**
 * @brief Pet quality levels (WoW battle pet system)
 */
enum class PetQuality : uint8
{
    POOR = 0,      // Grey
    COMMON = 1,    // White
    UNCOMMON = 2,  // Green
    RARE = 3,      // Blue
    EPIC = 4,      // Purple
    LEGENDARY = 5  // Orange
};

/**
 * @brief Pet family types
 */
enum class PetFamily : uint8
{
    HUMANOID = 1,
    DRAGONKIN = 2,
    FLYING = 3,
    UNDEAD = 4,
    CRITTER = 5,
    MAGIC = 6,
    ELEMENTAL = 7,
    BEAST = 8,
    AQUATIC = 9,
    MECHANICAL = 10
};

/**
 * @brief Battle pet information
 */
struct BattlePetInfo
{
    uint32 speciesId;
    std::string name;
    PetFamily family;
    uint32 level;
    uint32 xp;
    PetQuality quality;
    uint32 health;
    uint32 maxHealth;
    uint32 power;
    uint32 speed;
    bool isFavorite;
    bool isRare;
    bool isTradeable;
    std::vector<uint32> abilities; // Ability IDs

    BattlePetInfo() : speciesId(0), family(PetFamily::BEAST), level(1), xp(0),
        quality(PetQuality::COMMON), health(100), maxHealth(100), power(10), speed(10),
        isFavorite(false), isRare(false), isTradeable(true) {}
};

/**
 * @brief Pet team composition for battles
 */
struct PetTeam
{
    std::string teamName;
    std::vector<uint32> petSpeciesIds; // Up to 3 pets
    bool isActive;

    PetTeam() : isActive(false) {}
};

/**
 * @brief Pet battle automation profile
 */
struct PetBattleAutomationProfile
{
    bool autoBattle = true;           // Auto-engage pet battles
    bool autoLevel = true;             // Auto-level low-level pets
    bool collectRares = true;          // Prioritize capturing rare pets
    bool avoidDuplicates = false;      // Don't capture pets already owned
    uint32 maxPetLevel = 25;           // Max level to train pets to
    bool useOptimalAbilities = true;   // Use best ability rotation
    bool healBetweenBattles = true;    // Heal pets between battles
    uint32 minHealthPercent = 30;      // Min health % before healing

    PetBattleAutomationProfile() = default;
};

/**
 * @brief Metrics for pet battle manager
 */
struct PetMetrics
{
    ::std::atomic<uint32> petsCollected{0};
    ::std::atomic<uint32> battlesWon{0};
    ::std::atomic<uint32> battlesLost{0};
    ::std::atomic<uint32> battlesStarted{0};
    ::std::atomic<uint32> battlesForfeited{0};
    ::std::atomic<uint32> petsLeveled{0};
    ::std::atomic<uint32> petsSwitched{0};
    ::std::atomic<uint32> raresCapture{0};
    ::std::atomic<uint32> raresFound{0};
    ::std::atomic<uint32> raresCaptured{0};
    ::std::atomic<uint32> abilitiesUsed{0};
    ::std::atomic<uint64> totalXPGained{0};
    ::std::atomic<uint64> damageDealt{0};
    ::std::atomic<uint64> healingDone{0};

    PetMetrics() = default;
};

/**
 * @brief Battle pet ability information
 */
struct AbilityInfo
{
    uint32 abilityId;
    std::string name;
    PetFamily family;
    uint32 damage;
    uint32 cooldown;
    bool isMultiTurn;
};

/**
 * @brief Battle Pet Manager - Complete battle pet automation for bots
 *
 * **Phase 6.3: Per-Bot Instance Pattern (26th Manager)**
 *
 * Features:
 * - Battle pet collection
 * - Pet battle AI
 * - Pet leveling automation
 * - Pet team composition
 * - Rare pet tracking
 * - Pet quality assessment
 * - Automatic pet healing
 * - Optimal ability usage
 * - Performance optimized (per-bot isolation, zero mutex)
 *
 * **Ownership:**
 * - Owned by GameSystemsManager (26th manager)
 * - Each bot has independent pet collection and battle state
 * - Shared pet/ability database across all bots (static)
 */
class TC_GAME_API BattlePetManager final
{
public:
    // ============================================================================
    // CORE PET MANAGEMENT
    // ============================================================================

    /**
     * Initialize battle pet system on server startup
     */
    void Initialize();

    /**
     * Update pet automation for player (called periodically)
     */
    void Update(uint32 diff);

    /**
     * Get all pets player owns
     */
    std::vector<BattlePetInfo> GetPlayerPets() const;

    /**
     * Check if player owns pet
     */
    bool OwnsPet(uint32 speciesId) const;

    /**
     * Capture pet (after battle)
     */
    bool CapturePet(uint32 speciesId, PetQuality quality);

    /**
     * Release pet from collection
     */
    bool ReleasePet(uint32 speciesId);

    /**
     * Get pet count for player
     */
    uint32 GetPetCount() const;

    // ============================================================================
    // PET BATTLE AI
    // ============================================================================

    /**
     * Start pet battle
     */
    bool StartPetBattle(uint32 targetNpcId);

    /**
     * Execute pet battle turn
     */
    bool ExecuteBattleTurn();

    /**
     * Select best ability for current turn
     */
    uint32 SelectBestAbility() const;

    /**
     * Switch active pet during battle
     */
    bool SwitchActivePet(uint32 petIndex);

    /**
     * Use ability in battle
     */
    bool UseAbility(uint32 abilityId);

    /**
     * Check if should capture opponent pet
     */
    bool ShouldCapturePet() const;

    /**
     * Forfeit battle
     */
    bool ForfeitBattle();

    // ============================================================================
    // PET LEVELING
    // ============================================================================

    /**
     * Auto-level pets to max level
     */
    void AutoLevelPets();

    /**
     * Get pets that need leveling
     */
    std::vector<BattlePetInfo> GetPetsNeedingLevel() const;

    /**
     * Calculate XP required for next level
     */
    uint32 GetXPRequiredForLevel(uint32 currentLevel) const;

    /**
     * Award XP to pet after battle
     */
    void AwardPetXP(uint32 speciesId, uint32 xp);

    /**
     * Level up pet
     */
    bool LevelUpPet(uint32 speciesId);

    // ============================================================================
    // TEAM COMPOSITION
    // ============================================================================

    /**
     * Create pet team
     */
    bool CreatePetTeam(std::string const& teamName, std::vector<uint32> const& petSpeciesIds);

    /**
     * Get all pet teams
     */
    std::vector<PetTeam> GetPlayerTeams() const;

    /**
     * Set active team
     */
    bool SetActiveTeam(std::string const& teamName);

    /**
     * Get active team
     */
    PetTeam GetActiveTeam() const;

    /**
     * Optimize team composition based on opponent
     */
    std::vector<uint32> OptimizeTeamForOpponent(PetFamily opponentFamily) const;

    // ============================================================================
    // PET HEALING
    // ============================================================================

    /**
     * Heal all pets
     */
    bool HealAllPets();

    /**
     * Heal specific pet
     */
    bool HealPet(uint32 speciesId);

    /**
     * Check if pet needs healing
     */
    bool NeedsHealing(uint32 speciesId) const;

    /**
     * Get nearest pet healer NPC
     */
    uint32 FindNearestPetHealer() const;

    // ============================================================================
    // RARE PET TRACKING
    // ============================================================================

    /**
     * Track rare pet spawns
     */
    void TrackRarePetSpawns();

    /**
     * Check if pet is rare
     */
    bool IsRarePet(uint32 speciesId) const;

    /**
     * Get rare pets in current zone
     */
    std::vector<uint32> GetRarePetsInZone() const;

    /**
     * Navigate to rare pet spawn
     */
    bool NavigateToRarePet(uint32 speciesId);

    // ============================================================================
    // AUTOMATION PROFILES
    // ============================================================================

    void SetAutomationProfile(PetBattleAutomationProfile const& profile);
    PetBattleAutomationProfile GetAutomationProfile() const;

    // ============================================================================
    // METRICS
    // ============================================================================


    PetMetrics const& GetMetrics() const;
    PetMetrics const& GetGlobalMetrics() const;

public:
    /**
     * @brief Construct battle pet manager for specific bot
     * @param bot The bot player this manager serves
     */
    explicit BattlePetManager(Player* bot);
    ~BattlePetManager();

    // Non-copyable
    BattlePetManager(BattlePetManager const&) = delete;
    BattlePetManager& operator=(BattlePetManager const&) = delete;

private:

    // ============================================================================
    // INITIALIZATION HELPERS (static - operate on static data only)
    // ============================================================================

    static void LoadPetDatabase();
    static void InitializeAbilityDatabase();
    static void LoadRarePetList();

    // ============================================================================
    // BATTLE AI HELPERS
    // ============================================================================

    uint32 CalculateAbilityScore(uint32 abilityId, PetFamily opponentFamily) const;
    bool IsAbilityStrongAgainst(PetFamily abilityFamily, PetFamily opponentFamily) const;
    float CalculateTypeEffectiveness(PetFamily attackerFamily, PetFamily defenderFamily) const;
    bool ShouldSwitchPet() const;
    uint32 SelectBestSwitchTarget() const;

    /**
     * @brief Handle battle victory (award XP, capture pet, update metrics)
     */
    void OnBattleWon();

    // ============================================================================
    // DATA STRUCTURES
    // ============================================================================

    // Bot reference (non-owning)
    Player* _bot;

    // Per-bot instance data
    std::unordered_set<uint32> _ownedPets;              // Set of species IDs owned by this bot
    std::unordered_map<uint32, BattlePetInfo> _petInstances;  // speciesId -> pet info
    std::vector<PetTeam> _petTeams;                     // Pet teams for this bot
    std::string _activeTeam;                            // Currently active team name
    PetBattleAutomationProfile _profile;                // Automation settings
    PetMetrics _metrics;                                // Per-bot metrics
    uint32 _lastUpdateTime{0};                          // Last update timestamp

    // Battle state tracking
    bool _inBattle{false};                              // Currently in pet battle
    uint32 _battleStartTime{0};                         // When battle started (ms)
    uint32 _currentOpponentEntry{0};                    // Current opponent creature entry
    PetFamily _opponentFamily{PetFamily::BEAST};        // Opponent pet family
    uint32 _opponentLevel{1};                           // Opponent pet level
    float _opponentHealthPercent{100.0f};               // Opponent health percentage
    uint32 _opponentCurrentHealth{0};                   // Opponent current health
    uint32 _opponentMaxHealth{0};                       // Opponent max health
    std::unordered_map<uint32, uint32> _abilityCooldowns; // abilityId -> cooldown end time (ms)

    // Navigation tracking
    Position _navigationTarget;                         // Target position for navigation
    uint32 _navigationSpeciesId{0};                     // Species we're navigating to
    uint32 _pendingBattleTarget{0};                     // Queued battle target

    // Shared static data (all bots)
    static std::unordered_map<uint32, BattlePetInfo> _petDatabase;
    static std::unordered_map<uint32, std::vector<Position>> _rarePetSpawns;
    static std::unordered_map<uint32, AbilityInfo> _abilityDatabase;
    static PetMetrics _globalMetrics;
    static std::atomic<bool> _databaseInitialized;
    static std::once_flag _initFlag;

    // Update intervals
    static constexpr uint32 PET_UPDATE_INTERVAL = 5000;  // 5 seconds

    // Type effectiveness chart
    static constexpr float TYPE_STRONG = 1.5f;    // 50% bonus damage
    static constexpr float TYPE_WEAK = 0.67f;     // 33% reduced damage
    static constexpr float TYPE_NEUTRAL = 1.0f;   // Normal damage
};

} // namespace Playerbot
