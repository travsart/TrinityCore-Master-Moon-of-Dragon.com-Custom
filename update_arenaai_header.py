#!/usr/bin/env python3
"""Update ArenaAI.h with new method declarations"""

def update_header():
    filepath = 'C:/TrinityBots/TrinityCore/src/modules/Playerbot/PvP/ArenaAI.h'

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # Add new helper function declarations
    old_helpers = '''    // ============================================================================
    // HELPER FUNCTIONS
    // ============================================================================

    ArenaBracket GetArenaBracket() const;
    TeamComposition GetTeamComposition() const;
    TeamComposition GetEnemyTeamComposition() const;
    std::vector<::Player*> GetTeammates() const;
    std::vector<::Unit*> GetEnemyTeam() const;
    bool IsInLineOfSight(::Unit* target) const;
    float GetOptimalRangeForClass() const;
    bool IsTeammateInDanger(::Player* teammate) const;'''

    new_helpers = '''    // ============================================================================
    // HELPER FUNCTIONS
    // ============================================================================

    ArenaBracket GetArenaBracket() const;
    uint8 GetBracketTeamSize(ArenaBracket bracket) const;
    TeamComposition GetTeamComposition() const;
    TeamComposition GetEnemyTeamComposition() const;
    std::vector<::Player*> GetTeammates() const;
    std::vector<::Unit*> GetEnemyTeam() const;
    bool IsInLineOfSight(::Unit* target) const;
    float GetOptimalRangeForClass() const;
    bool IsTeammateInDanger(::Player* teammate) const;

    // ============================================================================
    // RATING SYSTEM HELPERS
    // ============================================================================

    /**
     * @brief Estimate opponent team's rating based on match state
     * @return Estimated opponent rating
     */
    uint32 EstimateOpponentRating() const;

    /**
     * @brief Record match result for performance tracking
     * @param won Whether the match was won
     * @param oldRating Rating before match
     * @param newRating Rating after match
     * @param opponentRating Estimated opponent rating
     * @param duration Match duration in seconds
     */
    void RecordMatchResult(bool won, uint32 oldRating, uint32 newRating,
        uint32 opponentRating, uint32 duration);

    // ============================================================================
    // TARGET ANALYSIS HELPERS
    // ============================================================================

    /**
     * @brief Calculate priority score for target selection
     * @param target The target to evaluate
     * @return Priority score (lower = higher priority)
     */
    float CalculateTargetPriorityScore(::Unit* target) const;

    /**
     * @brief Check if target is under pressure from teammates
     * @param target The target to check
     * @return True if target is being focused by team
     */
    bool IsTargetUnderTeamPressure(::Unit* target) const;

    /**
     * @brief Check if target has defensive cooldown active
     * @param target The target to check
     * @return True if target has major defensive active
     */
    bool HasDefensiveCooldownActive(::Unit* target) const;'''

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
