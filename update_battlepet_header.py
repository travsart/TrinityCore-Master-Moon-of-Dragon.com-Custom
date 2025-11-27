#!/usr/bin/env python3
"""Update BattlePetManager.h with new member variables for battle state tracking"""

def update_header():
    filepath = 'C:/TrinityBots/TrinityCore/src/modules/Playerbot/Companion/BattlePetManager.h'

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # Find and extend the private data structures section
    old_data = '''    // ============================================================================
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
    uint32 _lastUpdateTime{0};                          // Last update timestamp'''

    new_data = '''    // ============================================================================
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
    uint32 _pendingBattleTarget{0};                     // Queued battle target'''

    content = content.replace(old_data, new_data)

    # Add the OnBattleWon helper method declaration
    old_helpers = '''    // ============================================================================
    // BATTLE AI HELPERS
    // ============================================================================

    uint32 CalculateAbilityScore(uint32 abilityId, PetFamily opponentFamily) const;
    bool IsAbilityStrongAgainst(PetFamily abilityFamily, PetFamily opponentFamily) const;
    float CalculateTypeEffectiveness(PetFamily attackerFamily, PetFamily defenderFamily) const;
    bool ShouldSwitchPet() const;
    uint32 SelectBestSwitchTarget() const;'''

    new_helpers = '''    // ============================================================================
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
    void OnBattleWon();'''

    content = content.replace(old_helpers, new_helpers)

    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Successfully updated {filepath}")
        return True
    else:
        print(f"No changes needed for {filepath}")
        return False

if __name__ == '__main__':
    update_header()
