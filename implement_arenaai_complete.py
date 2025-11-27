#!/usr/bin/env python3
"""Complete enterprise-grade ArenaAI implementation"""

import re

def implement_arena_ai():
    filepath = 'C:/TrinityBots/TrinityCore/src/modules/Playerbot/PvP/ArenaAI.cpp'

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # 1. Add additional includes at the top
    old_includes = '''#include "ArenaAI.h"
#include "GameTime.h"
#include "Player.h"
#include "Unit.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Group.h"
#include "World.h"
#include <algorithm>
#include <cmath>'''

    new_includes = '''#include "ArenaAI.h"
#include "GameTime.h"
#include "Player.h"
#include "Unit.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Group.h"
#include "World.h"
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "DB2Stores.h"
#include "DBCEnums.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace
{
    // ============================================================================
    // ELO RATING SYSTEM CONSTANTS
    // ============================================================================

    // K-factor determines rating volatility
    // Higher K = more volatile ratings, faster adjustment
    constexpr int32 K_FACTOR_2V2 = 32;          // 2v2 has higher K for faster convergence
    constexpr int32 K_FACTOR_3V3 = 24;          // 3v3 is the competitive standard
    constexpr int32 K_FACTOR_5V5 = 20;          // 5v5 has lower K due to team size

    // Rating floors and caps
    constexpr uint32 RATING_FLOOR = 0;          // Minimum possible rating
    constexpr uint32 RATING_CAP = 3000;         // Maximum possible rating
    constexpr uint32 DEFAULT_RATING = 1500;     // Starting rating
    constexpr uint32 SOFT_CAP_RATING = 2400;    // Above this, gains are reduced

    // MMR spread limits
    constexpr int32 MAX_RATING_DIFFERENCE = 400; // Max considered difference for expected score

    // High rating modifier thresholds
    constexpr uint32 HIGH_RATING_THRESHOLD_1 = 2000;
    constexpr uint32 HIGH_RATING_THRESHOLD_2 = 2200;
    constexpr uint32 HIGH_RATING_THRESHOLD_3 = 2400;

    /**
     * @brief Calculate expected win probability using ELO formula
     * @param playerRating The player/team rating
     * @param opponentRating The opponent team rating
     * @return Expected score (probability of winning) between 0.0 and 1.0
     */
    float CalculateExpectedScore(int32 playerRating, int32 opponentRating)
    {
        // Clamp rating difference to prevent extreme expected scores
        int32 diff = std::clamp(opponentRating - playerRating, -MAX_RATING_DIFFERENCE, MAX_RATING_DIFFERENCE);

        // ELO expected score formula: E = 1 / (1 + 10^(D/400))
        // where D is the rating difference
        return 1.0f / (1.0f + std::pow(10.0f, static_cast<float>(diff) / 400.0f));
    }

    /**
     * @brief Get K-factor based on arena bracket
     * @param bracket The arena bracket type
     * @return K-factor for rating calculation
     */
    int32 GetKFactorForBracket(Playerbot::ArenaBracket bracket)
    {
        switch (bracket)
        {
            case Playerbot::ArenaBracket::BRACKET_2v2:
                return K_FACTOR_2V2;
            case Playerbot::ArenaBracket::BRACKET_3v3:
                return K_FACTOR_3V3;
            case Playerbot::ArenaBracket::BRACKET_5v5:
                return K_FACTOR_5V5;
            default:
                return K_FACTOR_3V3;
        }
    }

    /**
     * @brief Apply diminishing returns for high ratings
     * @param baseChange The base rating change before modifiers
     * @param currentRating The player's current rating
     * @param isWin Whether this is a win (true) or loss (false)
     * @return Modified rating change with diminishing returns applied
     */
    int32 ApplyHighRatingModifier(int32 baseChange, uint32 currentRating, bool isWin)
    {
        if (!isWin)
            return baseChange; // No reduction for losses

        // Apply diminishing returns for high-rated players
        float modifier = 1.0f;

        if (currentRating >= HIGH_RATING_THRESHOLD_3)
            modifier = 0.6f;  // 60% gains above 2400
        else if (currentRating >= HIGH_RATING_THRESHOLD_2)
            modifier = 0.75f; // 75% gains above 2200
        else if (currentRating >= HIGH_RATING_THRESHOLD_1)
            modifier = 0.9f;  // 90% gains above 2000

        return static_cast<int32>(baseChange * modifier);
    }

    /**
     * @brief Check if player has healer spec using ChrSpecializationEntry
     * @param player The player to check
     * @return True if player's primary spec is healer role
     */
    bool IsPlayerHealer(Player const* player)
    {
        if (!player)
            return false;

        ChrSpecializationEntry const* spec = player->GetPrimarySpecializationEntry();
        return spec && spec->GetRole() == ChrSpecializationRole::Healer;
    }

    /**
     * @brief Check if player has tank spec using ChrSpecializationEntry
     * @param player The player to check
     * @return True if player's primary spec is tank role
     */
    bool IsPlayerTank(Player const* player)
    {
        if (!player)
            return false;

        ChrSpecializationEntry const* spec = player->GetPrimarySpecializationEntry();
        return spec && spec->GetRole() == ChrSpecializationRole::Tank;
    }

    /**
     * @brief Check if player has DPS spec using ChrSpecializationEntry
     * @param player The player to check
     * @return True if player's primary spec is DPS role
     */
    bool IsPlayerDPS(Player const* player)
    {
        if (!player)
            return false;

        ChrSpecializationEntry const* spec = player->GetPrimarySpecializationEntry();
        return spec && spec->GetRole() == ChrSpecializationRole::Dps;
    }

    /**
     * @brief Get player's role from their specialization
     * @param player The player to check
     * @return The role (Tank=0, Healer=1, DPS=2)
     */
    ChrSpecializationRole GetPlayerRole(Player const* player)
    {
        if (!player)
            return ChrSpecializationRole::Dps;

        ChrSpecializationEntry const* spec = player->GetPrimarySpecializationEntry();
        if (spec)
            return spec->GetRole();

        // Fallback: assume DPS
        return ChrSpecializationRole::Dps;
    }

    /**
     * @brief Get arena map IDs for modern WoW arenas
     * @return Set of arena map IDs
     */
    std::unordered_set<uint32> const& GetArenaMapIds()
    {
        static std::unordered_set<uint32> arenaMapIds = {
            559,    // Nagrand Arena
            562,    // Blade's Edge Arena
            572,    // Ruins of Lordaeron
            617,    // Dalaran Arena
            618,    // Ring of Valor
            980,    // Tol'viron Arena
            1134,   // Tiger's Peak (Pandaria)
            1504,   // Black Rook Hold Arena (Legion)
            1505,   // Ashamane's Fall (Legion)
            1552,   // Robodrome (BFA)
            1672,   // The Robodrome
            1825,   // Hook Point (BFA)
            2167,   // Empyrean Domain (Shadowlands)
            2373,   // Enigma Crucible (Dragonflight)
            2509,   // Nokhudon Proving Grounds (Dragonflight)
            2547,   // Ashamane's Fall (Updated)
            2563,   // Maldraxxus Coliseum (Shadowlands)
        };
        return arenaMapIds;
    }
}'''

    content = content.replace(old_includes, new_includes)

    # 2. Replace OnMatchEnd with full rating calculation
    old_on_match_end = '''void ArenaAI::OnMatchEnd(bool won)
{
    if (!_bot)
        return;

    std::lock_guard lock(_mutex);
    // Update metrics
    if (won)
    {
        _metrics.matchesWon++;
        _globalMetrics.matchesWon++;

        // DESIGN NOTE: Simplified arena rating calculation
        // Current behavior: Fixed +15 rating gain for wins, -15 for losses
        // Full implementation should:
        // - Use TrinityCore's Battleground rating system (BattlegroundMgr, ArenaTeam)
        // - Calculate rating change based on MMR difference between teams (ELO-like system)
        // - Apply rating formula: ratingChange = K * (actualScore - expectedScore)
        // - Factor in team average rating vs opponent average rating
        // - Apply diminishing returns for rating gains at higher ratings
        // - Use different K-factors for 2v2, 3v3, 5v5 brackets
        // Reference: ArenaTeam::FinishGame(), BattlegroundMgr rating calculations
        _metrics.rating += 15;
    }
    else
    {
        _metrics.matchesLost++;
        _globalMetrics.matchesLost++;
        uint32 currentRating = _metrics.rating.load();
        if (currentRating > 15)
            _metrics.rating -= 15;
    }

    TC_LOG_INFO("playerbot", "ArenaAI: Match ended for player {} - {} (Rating: {})",
        _bot->GetGUID().GetCounter(), won ? "WON" : "LOST", _metrics.rating.load());

    // Clear match data
}'''

    new_on_match_end = '''void ArenaAI::OnMatchEnd(bool won)
{
    if (!_bot)
        return;

    std::lock_guard lock(_mutex);

    // Get current bracket for K-factor calculation
    ArenaBracket bracket = GetArenaBracket();
    int32 kFactor = GetKFactorForBracket(bracket);

    // Get current ratings
    uint32 currentRating = _metrics.rating.load();

    // Calculate opponent team average rating
    // In a real scenario, this would come from the battleground/arena data
    // For bots, we estimate based on match state and enemy composition
    uint32 opponentRating = EstimateOpponentRating();

    // Calculate expected score using ELO formula
    float expectedScore = CalculateExpectedScore(static_cast<int32>(currentRating),
                                                  static_cast<int32>(opponentRating));

    // Actual score: 1.0 for win, 0.0 for loss
    float actualScore = won ? 1.0f : 0.0f;

    // Calculate base rating change: K * (actual - expected)
    int32 baseRatingChange = static_cast<int32>(kFactor * (actualScore - expectedScore));

    // Apply high rating diminishing returns for wins
    int32 finalRatingChange = ApplyHighRatingModifier(baseRatingChange, currentRating, won);

    // Ensure minimum change of 1 for wins against lower-rated opponents
    if (won && finalRatingChange < 1)
        finalRatingChange = 1;

    // Ensure minimum loss of 1 for losses against higher-rated opponents
    if (!won && finalRatingChange > -1)
        finalRatingChange = -1;

    // Apply rating change with floor/cap protection
    int32 newRating = static_cast<int32>(currentRating) + finalRatingChange;
    newRating = std::clamp(newRating, static_cast<int32>(RATING_FLOOR), static_cast<int32>(RATING_CAP));
    _metrics.rating = static_cast<uint32>(newRating);

    // Update win/loss metrics
    if (won)
    {
        _metrics.matchesWon++;
        _globalMetrics.matchesWon++;
    }
    else
    {
        _metrics.matchesLost++;
        _globalMetrics.matchesLost++;
    }

    // Update match state statistics
    ArenaMatchState& state = _matchState;
    uint32 matchDuration = GetMatchDuration();

    TC_LOG_INFO("playerbot.arena", "ArenaAI: Match ended for player {} ({}) - {} | "
        "Rating: {} -> {} ({}{}), Bracket: {}v{}, Duration: {}s, "
        "Expected: {:.2f}, K-factor: {}",
        _bot->GetName(), _bot->GetGUID().GetCounter(),
        won ? "WON" : "LOST",
        currentRating, newRating,
        finalRatingChange >= 0 ? "+" : "", finalRatingChange,
        GetBracketTeamSize(bracket), GetBracketTeamSize(bracket),
        matchDuration,
        expectedScore, kFactor);

    // Record match history for performance analysis
    RecordMatchResult(won, currentRating, newRating, opponentRating, matchDuration);

    // Clear match-specific data
    _matchState = ArenaMatchState();
}'''

    content = content.replace(old_on_match_end, new_on_match_end)

    # 3. Replace SelectFocusTarget with proper spec detection
    old_select_focus = '''::Unit* ArenaAI::SelectFocusTarget() const
{
    if (!_bot)
        return nullptr;

    ArenaProfile profile = GetArenaProfile();
    std::vector<::Unit*> enemies = GetEnemyTeam();

    if (enemies.empty())
        return nullptr;

    switch (profile.strategy)
    {
        case ArenaStrategy::KILL_HEALER_FIRST:
        {
            // Find healer
            for (::Unit* enemy : enemies)
            {
                if (enemy->IsPlayer())
                {
                    ::Player* enemyPlayer = enemy->ToPlayer();
                    // DESIGN NOTE: Simplified healer detection
                    // Current behavior: Checks class only, assumes all Priests/Paladins/etc are healers
                    // Full implementation should:
                    // - Query player's active ChrSpecialization to determine if healing spec
                    // - Check for healing spec IDs specifically (Holy Priest, Holy Paladin, etc)
                    // - Account for dual-spec situations in arena
                    // - Verify player is actually specced as healer via GetPrimarySpecialization()
                    // Reference: IsHealer() method in PvPCombatAI for proper implementation pattern
                    uint8 enemyClass = enemyPlayer->GetClass();
                    if (enemyClass == CLASS_PRIEST || enemyClass == CLASS_PALADIN ||
                        enemyClass == CLASS_SHAMAN || enemyClass == CLASS_DRUID ||
                        enemyClass == CLASS_MONK || enemyClass == CLASS_EVOKER)
                        return enemy;
                }
            }
            break;
        }

        case ArenaStrategy::KILL_LOWEST_HEALTH:
        {
            // Find lowest health target
            ::Unit* lowestHealthTarget = nullptr;
            uint32 lowestHealth = 100;

            for (::Unit* enemy : enemies)
            {
                uint32 healthPct = enemy->GetHealthPct();
                if (healthPct < lowestHealth)
                {
                    lowestHealth = healthPct;
                    lowestHealthTarget = enemy;
                }
            }

            return lowestHealthTarget;
        }

        case ArenaStrategy::TRAIN_ONE_TARGET:
        {
            // Keep attacking same target
            if (_focusTargets.count(_bot->GetGUID().GetCounter()))
            {
                ObjectGuid targetGuid = _focusTarget;
                return ObjectAccessor::GetUnit(*_bot, targetGuid);
            }
            break;
        }

        default:
            break;
    }

    // Default: return first enemy
    return enemies.empty() ? nullptr : enemies[0];
}'''

    new_select_focus = '''::Unit* ArenaAI::SelectFocusTarget() const
{
    if (!_bot)
        return nullptr;

    ArenaProfile profile = GetArenaProfile();
    std::vector<::Unit*> enemies = GetEnemyTeam();

    if (enemies.empty())
        return nullptr;

    switch (profile.strategy)
    {
        case ArenaStrategy::KILL_HEALER_FIRST:
        {
            // Find healer using proper spec-based detection
            ::Unit* healerTarget = nullptr;
            float healerLowestHealth = 100.0f;

            for (::Unit* enemy : enemies)
            {
                if (enemy->IsPlayer())
                {
                    ::Player* enemyPlayer = enemy->ToPlayer();

                    // Use ChrSpecializationEntry to properly detect healer role
                    if (IsPlayerHealer(enemyPlayer))
                    {
                        // If multiple healers, prioritize lowest health
                        float healthPct = enemy->GetHealthPct();
                        if (healthPct < healerLowestHealth)
                        {
                            healerLowestHealth = healthPct;
                            healerTarget = enemy;
                        }
                    }
                }
            }

            if (healerTarget)
            {
                TC_LOG_DEBUG("playerbot.arena", "ArenaAI: Player {} targeting healer {} ({}% health)",
                    _bot->GetGUID().GetCounter(),
                    healerTarget->IsPlayer() ? healerTarget->ToPlayer()->GetName() : "Unknown",
                    static_cast<int>(healerLowestHealth));
                return healerTarget;
            }

            // Fallback: No healer found, use lowest health
            [[fallthrough]];
        }

        case ArenaStrategy::KILL_LOWEST_HEALTH:
        {
            // Find lowest health target with priority for targets under CC/pressure
            ::Unit* lowestHealthTarget = nullptr;
            float lowestHealth = 100.0f;
            float lowestPriorityScore = std::numeric_limits<float>::max();

            for (::Unit* enemy : enemies)
            {
                if (enemy->isDead())
                    continue;

                float healthPct = enemy->GetHealthPct();

                // Calculate priority score (lower = higher priority)
                // Base score is health percentage
                float priorityScore = healthPct;

                // Bonus priority for targets already being attacked
                if (enemy->GetVictim() == _bot || IsTargetUnderTeamPressure(enemy))
                    priorityScore -= 15.0f;

                // Bonus priority for targets in execute range
                if (healthPct < 20.0f)
                    priorityScore -= 20.0f;

                // Penalty for targets with defensive cooldowns active
                if (HasDefensiveCooldownActive(enemy))
                    priorityScore += 25.0f;

                // Bonus for targets in line of sight
                if (IsInLineOfSight(enemy))
                    priorityScore -= 10.0f;
                else
                    priorityScore += 50.0f; // Heavy penalty for out of LoS

                if (priorityScore < lowestPriorityScore)
                {
                    lowestPriorityScore = priorityScore;
                    lowestHealth = healthPct;
                    lowestHealthTarget = enemy;
                }
            }

            if (lowestHealthTarget)
            {
                TC_LOG_DEBUG("playerbot.arena", "ArenaAI: Player {} targeting lowest health {} ({}% health, score: {:.1f})",
                    _bot->GetGUID().GetCounter(),
                    lowestHealthTarget->IsPlayer() ? lowestHealthTarget->ToPlayer()->GetName() : "Unknown",
                    static_cast<int>(lowestHealth), lowestPriorityScore);
            }

            return lowestHealthTarget;
        }

        case ArenaStrategy::TRAIN_ONE_TARGET:
        {
            // Keep attacking same target if valid
            ObjectGuid targetGuid = _focusTarget;
            if (!targetGuid.IsEmpty())
            {
                ::Unit* currentTarget = ObjectAccessor::GetUnit(*_bot, targetGuid);
                if (currentTarget && !currentTarget->isDead() && IsInLineOfSight(currentTarget))
                    return currentTarget;
            }

            // Target died or invalid - select new train target
            // Prefer DPS targets for training strategy
            for (::Unit* enemy : enemies)
            {
                if (enemy->IsPlayer() && !enemy->isDead())
                {
                    ::Player* enemyPlayer = enemy->ToPlayer();
                    if (IsPlayerDPS(enemyPlayer) && IsInLineOfSight(enemy))
                    {
                        // Update focus target
                        const_cast<ArenaAI*>(this)->_focusTarget = enemy->GetGUID();
                        return enemy;
                    }
                }
            }
            break;
        }

        case ArenaStrategy::SPREAD_PRESSURE:
        {
            // Find target that hasn't been damaged recently
            ::Unit* freshTarget = nullptr;
            uint32 oldestDamageTime = GameTime::GetGameTimeMS();

            for (::Unit* enemy : enemies)
            {
                if (enemy->isDead() || !IsInLineOfSight(enemy))
                    continue;

                // Check if this target has been taking damage recently
                // Prefer targets not under pressure
                if (!IsTargetUnderTeamPressure(enemy))
                {
                    freshTarget = enemy;
                    break;
                }
            }

            if (freshTarget)
                return freshTarget;

            // Fallback to lowest health
            break;
        }

        case ArenaStrategy::ADAPTIVE:
        case ArenaStrategy::KILL_DPS_FIRST:
        {
            // Find DPS targets first
            ::Unit* dpsTarget = nullptr;
            float lowestDpsHealth = 100.0f;

            for (::Unit* enemy : enemies)
            {
                if (enemy->IsPlayer() && !enemy->isDead())
                {
                    ::Player* enemyPlayer = enemy->ToPlayer();
                    if (IsPlayerDPS(enemyPlayer))
                    {
                        float healthPct = enemy->GetHealthPct();
                        if (healthPct < lowestDpsHealth && IsInLineOfSight(enemy))
                        {
                            lowestDpsHealth = healthPct;
                            dpsTarget = enemy;
                        }
                    }
                }
            }

            if (dpsTarget)
                return dpsTarget;
            break;
        }

        default:
            break;
    }

    // Default: return first valid enemy in LoS
    for (::Unit* enemy : enemies)
    {
        if (!enemy->isDead() && IsInLineOfSight(enemy))
            return enemy;
    }

    return enemies.empty() ? nullptr : enemies[0];
}'''

    content = content.replace(old_select_focus, new_select_focus)

    # 4. Replace GetKillTargetPriority with proper spec detection
    old_kill_priority = '''std::vector<::Unit*> ArenaAI::GetKillTargetPriority() const
{
    std::vector<::Unit*> priorities;

    if (!_bot)
        return priorities;

    std::vector<::Unit*> enemies = GetEnemyTeam();
    // Sort by priority: Healers > Low health > DPS
    std::sort(enemies.begin(), enemies.end(),
        [](::Unit* a, ::Unit* b) {
            bool aIsHealer = false, bIsHealer = false;
            if (a->IsPlayer())
            {
                uint8 aClass = a->ToPlayer()->GetClass();
                aIsHealer = (aClass == CLASS_PRIEST || aClass == CLASS_PALADIN ||
                            aClass == CLASS_SHAMAN || aClass == CLASS_DRUID ||
                            aClass == CLASS_MONK || aClass == CLASS_EVOKER);
            }

            if (b->IsPlayer())
            {
                uint8 bClass = b->ToPlayer()->GetClass();
                bIsHealer = (bClass == CLASS_PRIEST || bClass == CLASS_PALADIN ||
                            bClass == CLASS_SHAMAN || bClass == CLASS_DRUID ||
                            bClass == CLASS_MONK || bClass == CLASS_EVOKER);
            }

            if (aIsHealer && !bIsHealer) return true;
            if (!aIsHealer && bIsHealer) return false;

            // If both same priority, sort by health
            return a->GetHealthPct() < b->GetHealthPct();
        });

    return enemies;
}'''

    new_kill_priority = '''std::vector<::Unit*> ArenaAI::GetKillTargetPriority() const
{
    std::vector<::Unit*> priorities;

    if (!_bot)
        return priorities;

    std::vector<::Unit*> enemies = GetEnemyTeam();

    // Filter out dead enemies
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
        [](::Unit* u) { return !u || u->isDead(); }), enemies.end());

    // Sort by priority score: Lower score = higher priority
    // Priority order: Healers in execute range > Healers > Low health DPS > Tanks
    std::sort(enemies.begin(), enemies.end(),
        [this](::Unit* a, ::Unit* b) {
            float aScore = CalculateTargetPriorityScore(a);
            float bScore = CalculateTargetPriorityScore(b);
            return aScore < bScore;
        });

    return enemies;
}

float ArenaAI::CalculateTargetPriorityScore(::Unit* target) const
{
    if (!target || !target->IsPlayer())
        return 1000.0f; // Non-players have lowest priority

    ::Player* player = target->ToPlayer();
    float score = 50.0f; // Base score

    // Role-based priority
    ChrSpecializationRole role = GetPlayerRole(player);
    switch (role)
    {
        case ChrSpecializationRole::Healer:
            score -= 30.0f; // Healers are high priority
            break;
        case ChrSpecializationRole::Tank:
            score += 20.0f; // Tanks are lower priority
            break;
        case ChrSpecializationRole::Dps:
            score += 0.0f; // DPS are neutral
            break;
    }

    // Health-based priority
    float healthPct = target->GetHealthPct();
    score += healthPct * 0.3f; // Lower health = higher priority

    // Execute range bonus (below 20%)
    if (healthPct < 20.0f)
        score -= 25.0f;
    else if (healthPct < 35.0f)
        score -= 10.0f;

    // LoS penalty
    if (!IsInLineOfSight(target))
        score += 100.0f;

    // Defensive cooldown penalty
    if (HasDefensiveCooldownActive(target))
        score += 30.0f;

    // Already being focused bonus
    if (IsTargetUnderTeamPressure(target))
        score -= 15.0f;

    // Distance consideration
    float distanceSq = _bot->GetExactDistSq(target);
    if (distanceSq > 40.0f * 40.0f)
        score += 20.0f; // Penalty for distant targets

    return score;
}'''

    content = content.replace(old_kill_priority, new_kill_priority)

    # 5. Replace GetArenaBracket with proper implementation
    old_get_bracket = '''ArenaBracket ArenaAI::GetArenaBracket() const
{
    if (!_bot)
        return ArenaBracket::BRACKET_2v2;

    // Full implementation: Query arena type
    // For now, default to 3v3
    return ArenaBracket::BRACKET_3v3;
}'''

    new_get_bracket = '''ArenaBracket ArenaAI::GetArenaBracket() const
{
    if (!_bot)
        return ArenaBracket::BRACKET_2v2;

    // Get the battleground the player is in
    Battleground* bg = _bot->GetBattleground();
    if (!bg || !bg->isArena())
        return ArenaBracket::BRACKET_3v3; // Default fallback

    // Get arena type from battleground
    uint8 arenaType = bg->GetArenaType();

    switch (arenaType)
    {
        case ARENA_TYPE_2v2:
            return ArenaBracket::BRACKET_2v2;
        case ARENA_TYPE_3v3:
            return ArenaBracket::BRACKET_3v3;
        case ARENA_TYPE_5v5:
            return ArenaBracket::BRACKET_5v5;
        default:
            break;
    }

    // Fallback: estimate from team size
    Group* group = _bot->GetGroup();
    if (group)
    {
        uint32 memberCount = group->GetMembersCount();
        if (memberCount <= 2)
            return ArenaBracket::BRACKET_2v2;
        else if (memberCount <= 3)
            return ArenaBracket::BRACKET_3v3;
        else
            return ArenaBracket::BRACKET_5v5;
    }

    return ArenaBracket::BRACKET_3v3;
}

uint8 ArenaAI::GetBracketTeamSize(ArenaBracket bracket) const
{
    switch (bracket)
    {
        case ArenaBracket::BRACKET_2v2: return 2;
        case ArenaBracket::BRACKET_3v3: return 3;
        case ArenaBracket::BRACKET_5v5: return 5;
        default: return 3;
    }
}'''

    content = content.replace(old_get_bracket, new_get_bracket)

    # 6. Replace GetTeamComposition with proper implementation
    old_team_comp = '''TeamComposition ArenaAI::GetTeamComposition() const
{
    if (!_bot)
        return TeamComposition::DOUBLE_DPS_HEALER;

    // Full implementation: Analyze team classes/specs
    return TeamComposition::DOUBLE_DPS_HEALER;
}'''

    new_team_comp = '''TeamComposition ArenaAI::GetTeamComposition() const
{
    if (!_bot)
        return TeamComposition::DOUBLE_DPS_HEALER;

    // Analyze team composition based on player specs
    std::vector<::Player*> teammates = GetTeammates();

    // Include self in analysis
    uint32 healerCount = IsPlayerHealer(_bot) ? 1 : 0;
    uint32 tankCount = IsPlayerTank(_bot) ? 1 : 0;
    uint32 dpsCount = IsPlayerDPS(_bot) ? 1 : 0;
    uint32 totalCount = 1;

    for (::Player* teammate : teammates)
    {
        if (!teammate)
            continue;

        totalCount++;

        if (IsPlayerHealer(teammate))
            healerCount++;
        else if (IsPlayerTank(teammate))
            tankCount++;
        else
            dpsCount++;
    }

    // Determine composition based on counts
    ArenaBracket bracket = GetArenaBracket();

    switch (bracket)
    {
        case ArenaBracket::BRACKET_2v2:
        {
            if (healerCount >= 1)
                return TeamComposition::DPS_HEALER;
            return TeamComposition::DOUBLE_DPS;
        }

        case ArenaBracket::BRACKET_3v3:
        {
            if (healerCount >= 1)
            {
                if (tankCount >= 1)
                    return TeamComposition::TANK_DPS_HEALER;
                return TeamComposition::DOUBLE_DPS_HEALER;
            }
            return TeamComposition::TRIPLE_DPS;
        }

        case ArenaBracket::BRACKET_5v5:
        {
            // 5v5 typically runs multiple healers
            if (healerCount >= 2)
                return TeamComposition::DOUBLE_DPS_HEALER; // Close approximation
            else if (healerCount >= 1)
            {
                if (tankCount >= 1)
                    return TeamComposition::TANK_DPS_HEALER;
                return TeamComposition::DOUBLE_DPS_HEALER;
            }
            return TeamComposition::TRIPLE_DPS;
        }

        default:
            break;
    }

    return TeamComposition::DOUBLE_DPS_HEALER;
}'''

    content = content.replace(old_team_comp, new_team_comp)

    # 7. Replace GetEnemyTeamComposition with proper implementation
    old_enemy_comp = '''TeamComposition ArenaAI::GetEnemyTeamComposition() const
{
    if (!_bot)
        return TeamComposition::DOUBLE_DPS_HEALER;

    // Full implementation: Analyze enemy team classes/specs
    return TeamComposition::DOUBLE_DPS_HEALER;
}'''

    new_enemy_comp = '''TeamComposition ArenaAI::GetEnemyTeamComposition() const
{
    if (!_bot)
        return TeamComposition::DOUBLE_DPS_HEALER;

    // Analyze enemy team composition
    std::vector<::Unit*> enemies = GetEnemyTeam();

    uint32 healerCount = 0;
    uint32 tankCount = 0;
    uint32 dpsCount = 0;
    uint32 totalCount = 0;

    for (::Unit* enemy : enemies)
    {
        if (!enemy || !enemy->IsPlayer())
            continue;

        ::Player* enemyPlayer = enemy->ToPlayer();
        totalCount++;

        if (IsPlayerHealer(enemyPlayer))
            healerCount++;
        else if (IsPlayerTank(enemyPlayer))
            tankCount++;
        else
            dpsCount++;
    }

    // Determine composition based on counts and bracket
    ArenaBracket bracket = GetArenaBracket();

    if (totalCount == 0)
        return TeamComposition::DOUBLE_DPS_HEALER; // Default assumption

    switch (bracket)
    {
        case ArenaBracket::BRACKET_2v2:
        {
            if (healerCount >= 1)
                return TeamComposition::DPS_HEALER;
            return TeamComposition::DOUBLE_DPS;
        }

        case ArenaBracket::BRACKET_3v3:
        {
            if (healerCount >= 1)
            {
                if (tankCount >= 1)
                    return TeamComposition::TANK_DPS_HEALER;
                return TeamComposition::DOUBLE_DPS_HEALER;
            }
            return TeamComposition::TRIPLE_DPS;
        }

        case ArenaBracket::BRACKET_5v5:
        {
            if (healerCount >= 2)
                return TeamComposition::DOUBLE_DPS_HEALER;
            else if (healerCount >= 1)
            {
                if (tankCount >= 1)
                    return TeamComposition::TANK_DPS_HEALER;
                return TeamComposition::DOUBLE_DPS_HEALER;
            }
            return TeamComposition::TRIPLE_DPS;
        }

        default:
            break;
    }

    return TeamComposition::DOUBLE_DPS_HEALER;
}'''

    content = content.replace(old_enemy_comp, new_enemy_comp)

    # 8. Replace GetEnemyTeam with proper implementation
    old_enemy_team = '''std::vector<::Unit*> ArenaAI::GetEnemyTeam() const
{
    std::vector<::Unit*> enemies;

    if (!_bot)
        return enemies;

    // Full implementation: Find all hostile players in arena
    return enemies;
}'''

    new_enemy_team = '''std::vector<::Unit*> ArenaAI::GetEnemyTeam() const
{
    std::vector<::Unit*> enemies;

    if (!_bot || !_bot->IsInWorld())
        return enemies;

    // Get battleground for arena-specific enemy detection
    Battleground* bg = _bot->GetBattleground();
    if (bg && bg->isArena())
    {
        // In arena, get all players from opposing team
        ::Team botTeam = _bot->GetBGTeam();
        ::Team enemyTeam = (botTeam == ALLIANCE) ? HORDE : ALLIANCE;

        // Iterate through arena players
        for (auto const& [guid, bgPlayer] : bg->GetPlayers())
        {
            if (bgPlayer.Team == enemyTeam)
            {
                ::Player* player = ObjectAccessor::FindPlayer(guid);
                if (player && player->IsInWorld() && !player->isDead() &&
                    player->IsAlive() && _bot->IsWithinDist(player, 100.0f))
                {
                    enemies.push_back(player);
                }
            }
        }
    }
    else
    {
        // Fallback: Find nearby hostile players
        std::list<::Player*> nearbyPlayers;
        _bot->GetPlayerListInGrid(nearbyPlayers, 100.0f);

        for (::Player* player : nearbyPlayers)
        {
            if (player && player != _bot &&
                player->IsHostileTo(_bot) &&
                !player->isDead() && player->IsAlive())
            {
                enemies.push_back(player);
            }
        }
    }

    // Sort by distance for consistent ordering
    std::sort(enemies.begin(), enemies.end(),
        [this](::Unit* a, ::Unit* b) {
            return _bot->GetExactDistSq(a) < _bot->GetExactDistSq(b);
        });

    return enemies;
}'''

    content = content.replace(old_enemy_team, new_enemy_team)

    # 9. Add helper methods before the closing namespace brace
    # Find the last closing brace of namespace
    old_ending = '''} // namespace Playerbot'''

    new_ending = '''// ============================================================================
// RATING SYSTEM HELPERS
// ============================================================================

uint32 ArenaAI::EstimateOpponentRating() const
{
    if (!_bot)
        return DEFAULT_RATING;

    // Get current match state
    ArenaMatchState const& state = _matchState;

    // Base estimate from our own rating
    uint32 baseEstimate = _metrics.rating.load();

    // Analyze opponent team composition for adjustment
    TeamComposition enemyComp = GetEnemyTeamComposition();
    std::vector<::Unit*> enemies = GetEnemyTeam();

    // Estimate based on number of enemies alive
    int32 adjustment = 0;

    // If we're losing, enemies are likely higher rated
    if (!state.isWinning && state.teammateAliveCount < state.enemyAliveCount)
        adjustment += 50;

    // If enemies have strong composition, estimate higher
    if (enemyComp == TeamComposition::DOUBLE_DPS_HEALER)
        adjustment += 25; // Standard comp suggests experience

    // Check enemy health levels - low health enemies might be lower skilled
    float avgEnemyHealth = 0.0f;
    uint32 aliveEnemies = 0;
    for (::Unit* enemy : enemies)
    {
        if (enemy && !enemy->isDead())
        {
            avgEnemyHealth += enemy->GetHealthPct();
            aliveEnemies++;
        }
    }

    if (aliveEnemies > 0)
    {
        avgEnemyHealth /= aliveEnemies;
        // If enemies are taking heavy damage, might be lower rated
        if (avgEnemyHealth < 50.0f)
            adjustment -= 25;
    }

    // Clamp the estimate to reasonable bounds
    int32 estimate = static_cast<int32>(baseEstimate) + adjustment;
    return static_cast<uint32>(std::clamp(estimate,
        static_cast<int32>(RATING_FLOOR),
        static_cast<int32>(RATING_CAP)));
}

void ArenaAI::RecordMatchResult(bool won, uint32 oldRating, uint32 newRating,
    uint32 opponentRating, uint32 duration)
{
    // Store match history for performance analysis
    // This can be expanded to store in database for persistent tracking

    TC_LOG_DEBUG("playerbot.arena", "ArenaAI: Recording match - Won: {}, Rating: {} -> {}, "
        "Opponent: {}, Duration: {}s",
        won ? "Yes" : "No", oldRating, newRating, opponentRating, duration);

    // Update global metrics
    _globalMetrics.matchesWon += won ? 1 : 0;
    _globalMetrics.matchesLost += won ? 0 : 1;
}

bool ArenaAI::IsTargetUnderTeamPressure(::Unit* target) const
{
    if (!target || !_bot)
        return false;

    // Check if target is being attacked by teammates
    std::vector<::Player*> teammates = GetTeammates();
    for (::Player* teammate : teammates)
    {
        if (teammate && teammate->GetTarget() == target->GetGUID())
            return true;
    }

    // Check if target is low health (indicating pressure)
    if (target->GetHealthPct() < 50.0f)
        return true;

    return false;
}

bool ArenaAI::HasDefensiveCooldownActive(::Unit* target) const
{
    if (!target)
        return false;

    // List of major defensive cooldown auras to check
    // These are spell IDs for common defensive abilities
    static const std::vector<uint32> defensiveAuras = {
        // Paladin
        642,    // Divine Shield
        498,    // Divine Protection

        // Mage
        45438,  // Ice Block

        // Rogue
        31224,  // Cloak of Shadows
        5277,   // Evasion

        // Hunter
        186265, // Aspect of the Turtle

        // Warrior
        871,    // Shield Wall
        12975,  // Last Stand
        118038, // Die by the Sword

        // Priest
        47585,  // Dispersion

        // Death Knight
        48792,  // Icebound Fortitude
        48707,  // Anti-Magic Shell

        // Druid
        22812,  // Barkskin
        61336,  // Survival Instincts

        // Monk
        115176, // Zen Meditation
        122278, // Dampen Harm

        // Demon Hunter
        196555, // Netherwalk
        187827, // Metamorphosis (Vengeance)

        // Shaman
        108271, // Astral Shift

        // Warlock
        104773, // Unending Resolve

        // Evoker
        363916, // Obsidian Scales
    };

    for (uint32 spellId : defensiveAuras)
    {
        if (target->HasAura(spellId))
            return true;
    }

    return false;
}

} // namespace Playerbot'''

    content = content.replace(old_ending, new_ending)

    # Write the modified content
    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Successfully implemented ArenaAI improvements in {filepath}")
        return True
    else:
        print(f"No changes needed for {filepath}")
        return False

if __name__ == '__main__':
    implement_arena_ai()
