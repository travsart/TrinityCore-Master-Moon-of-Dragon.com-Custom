/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * REFACTORED ClassAI - Combat Specialization Only
 *
 * This refactored version provides:
 * 1. Clean separation from BotAI - combat only, no base behavior override
 * 2. No movement control - handled by BotAI strategies
 * 3. No throttling of base UpdateAI - ensures smooth following
 * 4. Focus on class-specific combat mechanics only
 */

#pragma once

#include "BotAI.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "Position.h"
#include <memory>
#include <vector>
#include <unordered_map>

namespace Playerbot
{

// Forward declarations
class ActionPriorityQueue;
class CooldownManager;
class ResourceManager;
class CombatBehaviorIntegration; // Forward declaration within Playerbot namespace
struct RecommendedAction; // Forward declaration for combat behavior actions

/**
 * ClassAI - Base class for all class-specific COMBAT AI
 *
 * CRITICAL DESIGN PRINCIPLES:
 * 1. ClassAI is ONLY for combat specialization
 * 2. NEVER override UpdateAI() - use OnCombatUpdate() instead
 * 3. NEVER control movement - let BotAI strategies handle it
 * 4. NEVER throttle updates - causes following issues
 * 5. Focus ONLY on class-specific combat mechanics
 */
class TC_GAME_API ClassAI : public BotAI
{
public:
    ClassAI(Player* bot);
    virtual ~ClassAI();

    // ========================================================================
    // COMBAT UPDATE - Called by BotAI::UpdateAI() when in combat
    // ========================================================================

    /**
     * Override from BotAI - handles class-specific combat updates
     * Called ONLY when bot is in combat state
     *
     * @param diff Time since last update in milliseconds
     */
    void OnCombatUpdate(uint32 diff) override;

    // ========================================================================
    // PURE VIRTUAL COMBAT INTERFACE - Must be implemented by each class
    // ========================================================================

    /**
     * Execute class-specific combat rotation
     * @param target Current combat target
     */
    virtual void UpdateRotation(::Unit* target) = 0;

    /**
     * Apply class-specific buffs
     * Called when not in combat or between combats
     */
    virtual void UpdateBuffs() = 0;

    /**
     * Update class-specific cooldown tracking
     * @param diff Time since last update
     */
    virtual void UpdateCooldowns(uint32 diff);

    /**
     * Check if a specific ability can be used
     * @param spellId Spell ID to check
     * @return true if ability can be used
     */
    virtual bool CanUseAbility(uint32 spellId);

    // ========================================================================
    // COMBAT STATE MANAGEMENT - Lifecycle hooks
    // ========================================================================

    /**
     * Called when entering combat
     * @param target Initial combat target
     */
    void OnCombatStart(::Unit* target) override;

    /**
     * Called when leaving combat
     */
    void OnCombatEnd() override;

    /**
     * Called when switching targets during combat
     * @param newTarget New combat target
     */
    virtual void OnTargetChanged(::Unit* newTarget);

    // ========================================================================
    // COMBAT BEHAVIOR INTEGRATION - Access to unified combat system
    // ========================================================================

    /**
     * Get combat behavior integration system
     * @return Pointer to combat behaviors (nullptr if not initialized)
     */
    CombatBehaviorIntegration* GetCombatBehaviors() { return _combatBehaviors.get(); }
    const CombatBehaviorIntegration* GetCombatBehaviors() const { return _combatBehaviors.get(); }

    /**
     * Check if combat behaviors are available
     * @return true if combat behavior system is initialized
     */
    bool HasCombatBehaviors() const { return _combatBehaviors != nullptr; }

    /**
     * Execute a recommended action from the combat behavior system
     * @param action The recommended action to execute
     * @return true if the action was successfully executed
     */
    bool ExecuteRecommendedAction(const RecommendedAction& action);

    // ========================================================================
    // POSITIONING - Combat positioning preferences (PUBLIC for Strategy access)
    // ========================================================================

    /**
     * Get optimal position for engaging target
     * NOTE: This only provides preference, actual movement is handled by BotAI
     *
     * @param target Combat target
     * @return Preferred position
     */
    virtual Position GetOptimalPosition(::Unit* target);

    /**
     * Get optimal range for class
     * @param target Combat target
     * @return Preferred combat range
     */
    virtual float GetOptimalRange(::Unit* target)
    {
        // Default implementation for base ClassAI
        // Melee classes (Warrior, Rogue, DK, etc.) use 5.0f
        // Ranged/Casters use 25.0f
        if (!GetBot())
            return 25.0f;

        uint8 botClass = GetBot()->GetClass();
        switch (botClass)
        {
            case CLASS_WARRIOR:
            case CLASS_ROGUE:
            case CLASS_DEATH_KNIGHT:
            case CLASS_MONK:        // Melee DPS/Tank specs
                return 5.0f;        // Melee range

            case CLASS_HUNTER:
            case CLASS_MAGE:
            case CLASS_WARLOCK:
            case CLASS_PRIEST:
            case CLASS_SHAMAN:
            case CLASS_DRUID:
            case CLASS_PALADIN:     // Hybrid, prefer ranged for safety
                return 25.0f;       // Ranged/casting range

            default:
                return 25.0f;       // Safe default for unknown classes
        }
    }

    // ========================================================================
    // UTILITY FUNCTIONS - Helpers for derived classes
    // ========================================================================

    // Spell readiness checks
    bool IsSpellReady(uint32 spellId);
    bool IsInRange(::Unit* target, uint32 spellId);
    bool HasLineOfSight(::Unit* target);
    bool IsSpellUsable(uint32 spellId);
    float GetSpellRange(uint32 spellId);
    uint32 GetSpellCooldown(uint32 spellId);

    // Spell casting
    bool CastSpell(::Unit* target, uint32 spellId);
    bool CastSpell(uint32 spellId); // Self-cast

    // Target selection helpers
    ::Unit* GetBestAttackTarget();
    ::Unit* GetBestHealTarget();
    ::Unit* GetNearestEnemy(float maxRange = 30.0f);
    ::Unit* GetLowestHealthAlly(float maxRange = 40.0f);

    // Buff/debuff utilities
    bool HasAura(uint32 spellId, ::Unit* target = nullptr);
    uint32 GetAuraStacks(uint32 spellId, ::Unit* target = nullptr);
    uint32 GetAuraRemainingTime(uint32 spellId, ::Unit* target = nullptr);

    // Movement queries (read-only - no control)
    bool IsMoving() const;
    bool IsInMeleeRange(::Unit* target) const;
    bool ShouldMoveToTarget(::Unit* target) const;
    float GetDistanceToTarget(::Unit* target) const;

protected:
    // ========================================================================
    // RESOURCE MANAGEMENT - Class-specific resources
    // ========================================================================

    /**
     * Check if bot has enough resources for a spell
     * @param spellId Spell to check
     * @return true if sufficient resources
     */
    virtual bool HasEnoughResource(uint32 spellId) = 0;

    /**
     * Consume resources for a spell cast
     * @param spellId Spell being cast
     */
    virtual void ConsumeResource(uint32 spellId) = 0;

    // ========================================================================
    // COMPONENT MANAGERS - Class-specific systems
    // ========================================================================

    std::unique_ptr<ActionPriorityQueue> _actionQueue;
    std::unique_ptr<CooldownManager> _cooldownManager;
    std::unique_ptr<ResourceManager> _resourceManager;

    // Combat Behavior Integration - Unified combat coordination system
    std::unique_ptr<CombatBehaviorIntegration> _combatBehaviors;

    // ========================================================================
    // COMBAT STATE - Current combat information
    // ========================================================================

    ::Unit* _currentCombatTarget;
    bool _inCombat;
    uint32 _combatTime;
    uint32 _lastTargetSwitch;

    // ========================================================================
    // PERFORMANCE OPTIMIZATION - Optional throttling for expensive operations
    // ========================================================================

    // CRITICAL: This throttle is ONLY for expensive operations like
    // buff checking or complex calculations. It MUST NOT affect:
    // - Basic rotation updates
    // - Target selection
    // - Critical ability usage

    uint32 _lastExpensiveUpdate = 0;
    static constexpr uint32 EXPENSIVE_UPDATE_INTERVAL = 500; // 500ms for expensive checks

private:
    // ========================================================================
    // INTERNAL METHODS - Called by OnCombatUpdate()
    // ========================================================================

    /**
     * Update combat targeting
     * Selects best target based on threat, health, positioning
     */
    void UpdateTargeting();

    /**
     * Update combat state tracking
     * @param diff Time since last update
     */
    void UpdateCombatState(uint32 diff);

    /**
     * Record performance metrics for analysis
     * @param metric Metric name
     * @param value Metric value
     */
    void RecordPerformanceMetric(std::string const& metric, uint32 value);
};

} // namespace Playerbot