/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Template-Based Combat Specialization Architecture
 *
 * This file provides the complete template architecture for eliminating
 * code duplication across all 40+ combat specializations.
 *
 * Design Goals:
 * - Eliminate 1,740+ duplicate method implementations
 * - Zero runtime overhead through compile-time template resolution
 * - Type-safe resource management for all resource types
 * - Thread-safe concurrent bot updates
 * - Maintain backward compatibility during migration
 */

#pragma once

#include "Define.h"
#include "ClassAI.h"
#include "ResourceTypes.h"
#include "Position.h"
#include "SpellMgr.h"
#include "ObjectAccessor.h"
#include "Cells/CellImpl.h"
#include "GridNotifiers.h"
#include "Log.h"
#include "Group.h"
#include <unordered_map>
#include <concepts>
#include <type_traits>
#include <atomic>
#include <shared_mutex>
#include <array>
#include <span>
#include <ranges>

namespace Playerbot
{

// ============================================================================
// RESOURCE TYPE CONCEPTS - C++20 Concepts for type safety
// ============================================================================

template<typename T>
concept SimpleResource = std::integral<T> && sizeof(T) <= 4;

template<typename T>
concept ComplexResource = requires(T t) {
    { t.available } -> std::convertible_to<bool>;
    { t.Consume(1u) } -> std::same_as<bool>;
    { t.Regenerate(1u) } -> std::same_as<void>;
    { t.GetAvailable() } -> std::convertible_to<uint32>;
    { t.GetMax() } -> std::convertible_to<uint32>;
};

template<typename T>
concept ValidResource = SimpleResource<T> || ComplexResource<T>;

// ============================================================================
// RESOURCE TRAITS - Compile-time resource type information
// ============================================================================

template<typename T>
struct ResourceTraits
{
    static constexpr bool is_simple = SimpleResource<T>;
    static constexpr bool is_complex = ComplexResource<T>;
    static constexpr bool regenerates = true;
    static constexpr uint32 regen_rate_ms = 2000;
    static constexpr float critical_threshold = 0.2f;
    static constexpr const char* name = "Unknown";
};

// Specializations for each resource type
template<>
struct ResourceTraits<uint32> // Mana, Rage, Energy, Focus
{
    static constexpr bool is_simple = true;
    static constexpr bool is_complex = false;
    static constexpr bool regenerates = true;
    static constexpr uint32 regen_rate_ms = 2000;
    static constexpr float critical_threshold = 0.2f;
    static constexpr const char* name = "Generic";
};

// Forward declaration for complex rune system
struct RuneSystem;

template<>
struct ResourceTraits<RuneSystem>
{
    static constexpr bool is_simple = false;
    static constexpr bool is_complex = true;
    static constexpr bool regenerates = true;
    static constexpr uint32 regen_rate_ms = 10000;
    static constexpr float critical_threshold = 0.33f; // Need at least 2 runes
    static constexpr const char* name = "Runes";
};

// ============================================================================
// BASE COMBAT SPECIALIZATION TEMPLATE
// ============================================================================

template<typename ResourceType>
    requires ValidResource<ResourceType>
class CombatSpecializationTemplate : public ClassAI
{
public:
    // Bring base class methods into scope for derived templates
    using ClassAI::GetBot;
    using ClassAI::CastSpell;
    using ClassAI::IsSpellReady;

    explicit CombatSpecializationTemplate(Player* botPtr)
        : ClassAI(botPtr)
        , bot(botPtr)  // Initialize bot reference for refactored specs
        , _resource{}
        , _maxResource(100)
        , _lastResourceUpdate(0)
        , _globalCooldownEnd(0)
        , _performanceMetrics{}
    {
        InitializeResource();
    }

    virtual ~CombatSpecializationTemplate() = default;

protected:
    // Convenience reference for refactored specializations
    // This allows derived classes to use 'bot' directly instead of calling GetBot()
    Player* const bot;

    // ========================================================================
    // FINAL METHODS - Cannot be overridden (eliminates duplication)
    // ========================================================================

    /**
     * Update all cooldowns - FINAL implementation for all specs
     * This eliminates 50+ duplicate implementations
     */
    void UpdateCooldowns(uint32 diff) final override
    {
        // Thread-safe cooldown update
        std::lock_guard<std::recursive_mutex> lock(_cooldownMutex);

        // Update global cooldown
        if (_globalCooldownEnd > diff)
            _globalCooldownEnd -= diff;
        else
            _globalCooldownEnd = 0;

        // Update ability cooldowns using ranges (C++20)
        for (auto& [spellId, cooldown] : _cooldowns)
        {
            if (cooldown > diff)
                cooldown -= diff;
            else
                cooldown = 0;
        }

        // Update buff timers
        std::erase_if(_activeBuffs, [diff](auto& pair) {
            pair.second = (pair.second > diff) ? pair.second - diff : 0;
            return pair.second == 0;
        });

        // Update DoT timers
        for (auto& [targetGuid, dots] : _activeDots)
        {
            std::erase_if(dots, [diff](auto& pair) {
                pair.second = (pair.second > diff) ? pair.second - diff : 0;
                return pair.second == 0;
            });
        }

        // Clean up empty DoT entries
        std::erase_if(_activeDots, [](const auto& pair) {
            return pair.second.empty();
        });

        _performanceMetrics.cooldownUpdates++;
    }

    /**
     * Check if ability can be used - FINAL implementation
     * This eliminates 50+ duplicate implementations
     */
    bool CanUseAbility(uint32 spellId) final override
    {
        // Thread-safe ability check
        std::lock_guard<std::recursive_mutex> lock(_cooldownMutex);

        // Check global cooldown
        if (_globalCooldownEnd > 0)
            return false;

        // Check specific cooldown
        if (auto it = _cooldowns.find(spellId); it != _cooldowns.end() && it->second > 0)
            return false;

        // Check resource requirement
        uint32 cost = GetSpellResourceCost(spellId);
        if (!HasEnoughResourceInternal(cost))
            return false;

        // Check if bot has the spell
        if (!GetBot()->HasSpell(spellId))
            return false;

        // Check if currently casting/channeling
        if (GetBot()->IsNonMeleeSpellCast(false, true))
            return false;

        _performanceMetrics.abilityChecks++;
        return true;
    }

    /**
     * Register a cooldown for a spell (used in specialization constructors)
     */
    void RegisterCooldown(uint32 spellId, uint32 cooldownMs)
    {
        std::lock_guard<std::recursive_mutex> lock(_cooldownMutex);
        _cooldowns[spellId] = 0; // Initialize to 0, will be set when spell is cast
        // Store the cooldown duration for reference
        _cooldownDurations[spellId] = cooldownMs;
    }

    /**
     * Combat state management - FINAL implementation
     * This eliminates 50+ duplicate implementations
     */
    void OnCombatStart(::Unit* target) final override
    {
        ClassAI::OnCombatStart(target);

        _combatStartTime = getMSTime();
        _currentTarget = target;
        _consecutiveFailedCasts = 0;

        // Reset performance metrics for this combat
        _performanceMetrics.combatStartTime = std::chrono::steady_clock::now();
        _performanceMetrics.totalCasts = 0;
        _performanceMetrics.failedCasts = 0;

        // Call specialization-specific combat start logic
        OnCombatStartSpecific(target);

        TC_LOG_DEBUG("module.playerbot", "Bot {} entered combat with {} (Resource: {}/{})",
            GetBot()->GetName(), target->GetName(),
            GetCurrentResourceInternal(), _maxResource);
    }

    void OnCombatEnd() final override
    {
        ClassAI::OnCombatEnd();

        uint32 combatDuration = getMSTime() - _combatStartTime;
        _performanceMetrics.totalCombatTime += std::chrono::milliseconds(combatDuration);

        // Clean up combat state
        _currentTarget = nullptr;
        _consecutiveFailedCasts = 0;

        // Clean up DoTs for dead targets
        CleanupExpiredDots();

        // Call specialization-specific combat end logic
        OnCombatEndSpecific();

        TC_LOG_DEBUG("module.playerbot", "Bot {} left combat (Duration: {}ms, Casts: {}, Failed: {})",
            GetBot()->GetName(), combatDuration,
            _performanceMetrics.totalCasts.load(), _performanceMetrics.failedCasts.load());
    }

    // ========================================================================
    // RESOURCE MANAGEMENT - Template-based for all resource types
    // ========================================================================

    bool HasEnoughResource(uint32 spellId) final override
    {
        uint32 cost = GetSpellResourceCost(spellId);
        return HasEnoughResourceInternal(cost);
    }

    void ConsumeResource(uint32 spellId) final override
    {
        uint32 cost = GetSpellResourceCost(spellId);
        ConsumeResourceInternal(cost);
        _performanceMetrics.resourceConsumed += cost;
    }

    // ========================================================================
    // VIRTUAL HOOKS - Can be overridden for specialization-specific logic
    // ========================================================================

protected:
    /**
     * Hook for specialization-specific combat start logic
     */
    virtual void OnCombatStartSpecific(::Unit* target) {}

    /**
     * Hook for specialization-specific combat end logic
     */
    virtual void OnCombatEndSpecific() {}

    /**
     * Get resource cost for a spell (can be overridden for special cases)
     */
    virtual uint32 GetSpellResourceCost(uint32 spellId) const
    {
        // Default implementation - can be specialized
        if (auto spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
        {
            // Get mana cost from power cost data
            auto costs = spellInfo->CalcPowerCost(GetBot(), spellInfo->GetSchoolMask());
            for (auto const& cost : costs)
            {
                if (cost.Power == POWER_MANA)
                    return cost.Amount;
            }
        }
        return 0;
    }

    // ========================================================================
    // INTERNAL RESOURCE HANDLING - Specialized for simple/complex types
    // ========================================================================

private:
    /**
     * Initialize resource based on type
     */
    void InitializeResource()
    {
        if constexpr (SimpleResource<ResourceType>)
        {
            _resource = static_cast<ResourceType>(_maxResource);
        }
        else if constexpr (ComplexResource<ResourceType>)
        {
            _resource.Initialize(GetBot());
        }
    }

    /**
     * Check if we have enough resources
     */
    bool HasEnoughResourceInternal(uint32 amount) const
    {
        if constexpr (SimpleResource<ResourceType>)
        {
            return _resource >= static_cast<ResourceType>(amount);
        }
        else if constexpr (ComplexResource<ResourceType>)
        {
            return _resource.GetAvailable() >= amount;
        }
    }

    /**
     * Consume resources
     */
    void ConsumeResourceInternal(uint32 amount)
    {
        std::lock_guard<std::recursive_mutex> lock(_resourceMutex);

        if constexpr (SimpleResource<ResourceType>)
        {
            if (_resource >= static_cast<ResourceType>(amount))
                _resource -= static_cast<ResourceType>(amount);
        }
        else if constexpr (ComplexResource<ResourceType>)
        {
            _resource.Consume(amount);
        }
    }

    /**
     * Get current resource amount
     */
    uint32 GetCurrentResourceInternal() const
    {
        if constexpr (SimpleResource<ResourceType>)
        {
            return static_cast<uint32>(_resource);
        }
        else if constexpr (ComplexResource<ResourceType>)
        {
            return _resource.GetAvailable();
        }
    }

    /**
     * Regenerate resources
     */
    void RegenerateResource(uint32 diff)
    {
        if constexpr (ResourceTraits<ResourceType>::regenerates)
        {
            std::lock_guard<std::recursive_mutex> lock(_resourceMutex);

            if constexpr (SimpleResource<ResourceType>)
            {
                // Simple regeneration
                uint32 regenAmount = diff * 5 / 1000; // 5 per second
                _resource = std::min<ResourceType>(_resource + regenAmount, _maxResource);
            }
            else if constexpr (ComplexResource<ResourceType>)
            {
                _resource.Regenerate(diff);
            }
        }
    }

    /**
     * Clean up expired DoTs
     */
    void CleanupExpiredDots()
    {
        std::lock_guard<std::recursive_mutex> lock(_cooldownMutex);

        // Remove DoTs for dead or invalid targets
        Player* bot = GetBot();
        std::erase_if(_activeDots, [bot](const auto& pair) {
            Unit* target = ObjectAccessor::GetUnit(*bot, pair.first);
            return !target || !target->IsAlive();
        });
    }

protected:
    /**
     * Helper method for refactored specializations - check if spell can be cast
     * This bridges the gap between old CanCastSpell() calls and new architecture
     */
    bool CanCastSpell(uint32 spellId, ::Unit* target = nullptr)
    {
        // Use CanUseAbility for basic checks
        if (!CanUseAbility(spellId))
            return false;

        // If target provided, check additional target-specific conditions
        if (target)
        {
            // Check if target is valid
            if (!target || !target->IsAlive() || target->IsFriendlyTo(GetBot()))
                return false;

            // Check if in range (use spell info to get range)
            if (auto spellInfo = sSpellMgr->GetSpellInfo(spellId, GetBot()->GetMap()->GetDifficultyID()))
            {
                float range = spellInfo->GetMaxRange(false, GetBot(), nullptr);
                if (GetBot()->GetDistance(target) > range)
                    return false;
            }

            // Check if line of sight
            if (!GetBot()->IsWithinLOSInMap(target))
                return false;
        }

        return true;
    }

    /**
     * Get number of enemies in range (for AoE decision making)
     */
    uint32 GetEnemiesInRange(float range) const
    {
        Player* bot = GetBot();
        if (!bot)
            return 0;

        std::list<Unit*> targets;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(bot, bot, range);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, u_check);
        Cell::VisitAllObjects(bot, searcher, range);
        return static_cast<uint32>(targets.size());
    }

    /**
     * Check if unit is behind target (for positional requirements)
     */
    bool IsBehindTarget(::Unit* target) const
    {
        if (!target)
            return false;
        return !target->HasInArc(static_cast<float>(M_PI), GetBot());
    }

protected:
    // Resource storage (either simple uint32 or complex type)
    ResourceType _resource;
    uint32 _maxResource;
    uint32 _lastResourceUpdate;

    // Cooldown tracking (thread-safe)
    mutable std::recursive_mutex _cooldownMutex;
    std::unordered_map<uint32, uint32> _cooldowns;
    std::unordered_map<uint32, uint32> _cooldownDurations; // Registered cooldown durations
    std::atomic<uint32> _globalCooldownEnd;

    // Buff and DoT tracking
    std::unordered_map<uint32, uint32> _activeBuffs; // spellId -> remaining duration
    std::unordered_map<ObjectGuid, std::unordered_map<uint32, uint32>> _activeDots; // targetGuid -> (spellId -> duration)

    // Resource management (thread-safe)
    mutable std::recursive_mutex _resourceMutex;

    // Combat state
    ::Unit* _currentTarget = nullptr;
    uint32 _combatStartTime = 0;
    std::atomic<uint32> _consecutiveFailedCasts{0};

    // Performance metrics
    struct PerformanceMetrics
    {
        std::atomic<uint32> totalCasts{0};
        std::atomic<uint32> failedCasts{0};
        std::atomic<uint32> resourceConsumed{0};
        std::atomic<uint32> resourceWasted{0};
        std::atomic<uint32> cooldownUpdates{0};
        std::atomic<uint32> abilityChecks{0};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::milliseconds totalCombatTime{0};
    } _performanceMetrics;

    // Constants
    static constexpr uint32 GLOBAL_COOLDOWN_MS = 1500;
    static constexpr uint32 MAX_FAILED_CASTS = 5;
};

// ============================================================================
// ROLE-BASED TEMPLATE SPECIALIZATIONS
// ============================================================================

/**
 * Melee DPS Specialization Template
 * Provides melee-specific defaults and behavior
 */
template<typename ResourceType>
    requires ValidResource<ResourceType>
class MeleeDpsSpecialization : public CombatSpecializationTemplate<ResourceType>
{
public:
    explicit MeleeDpsSpecialization(Player* bot)
        : CombatSpecializationTemplate<ResourceType>(bot)
    {
    }

    float GetOptimalRange(::Unit* target) override final
    {
        return 5.0f; // Melee range
    }

protected:
    Position GetOptimalPosition(::Unit* target) override
    {
        // Prefer behind target for melee DPS
        if (target)
        {
            float angle = target->GetOrientation() + M_PI; // Behind target
            float distance = 3.0f; // Close but not too close

            Position pos;
            pos.m_positionX = target->GetPositionX() + cos(angle) * distance;
            pos.m_positionY = target->GetPositionY() + sin(angle) * distance;
            pos.m_positionZ = target->GetPositionZ();
            pos.SetOrientation(target->GetAbsoluteAngle(&pos));

            return pos;
        }
        return this->GetBot()->GetPosition();
    }

    /**
     * Check if we can backstab/ambush
     */
    bool CanAttackFromBehind(::Unit* target) const
    {
        if (!target)
            return false;

        float angle = target->GetRelativeAngle(this->GetBot());
        return (angle > 2.96f || angle < -2.96f); // ~170 degrees behind
    }

    /**
     * Handle positioning for maximum DPS
     */
    virtual void OptimizePositioning(::Unit* target)
    {
        if (!CanAttackFromBehind(target))
        {
            // Request movement to behind target
            Position optimal = GetOptimalPosition(target);
            // Movement would be handled by BotAI, not here
        }
    }
};

/**
 * Ranged DPS Specialization Template
 * Provides ranged-specific defaults and behavior
 */
template<typename ResourceType>
    requires ValidResource<ResourceType>
class RangedDpsSpecialization : public CombatSpecializationTemplate<ResourceType>
{
public:
    explicit RangedDpsSpecialization(Player* bot)
        : CombatSpecializationTemplate<ResourceType>(bot)
        , _kiteDistance(25.0f)
        , _minimumRange(8.0f)
    {
    }

    float GetOptimalRange(::Unit* target) override final
    {
        return 25.0f; // Optimal ranged distance
    }

protected:
    Position GetOptimalPosition(::Unit* target) override
    {
        if (target)
        {
            // Maintain optimal distance for ranged DPS
            float currentDistance = this->GetBot()->GetDistance(target);

            if (currentDistance < _minimumRange)
            {
                // Too close, need to move back (kite)
                return GetKitePosition(target);
            }
            else if (currentDistance > GetOptimalRange(target))
            {
                // Too far, move closer
                return GetApproachPosition(target);
            }
        }
        return this->GetBot()->GetPosition();
    }

    /**
     * Get position for kiting away from target
     */
    Position GetKitePosition(::Unit* target) const
    {
        float angle = this->GetBot()->GetRelativeAngle(target) + M_PI; // Away from target

        Position pos;
        pos.m_positionX = this->GetBot()->GetPositionX() + cos(angle) * _kiteDistance;
        pos.m_positionY = this->GetBot()->GetPositionY() + sin(angle) * _kiteDistance;
        pos.m_positionZ = this->GetBot()->GetPositionZ();
        pos.SetOrientation(this->GetBot()->GetRelativeAngle(target));

        return pos;
    }

    /**
     * Get position for approaching target
     */
    Position GetApproachPosition(::Unit* target) const
    {
        float angle = this->GetBot()->GetRelativeAngle(target);
        // Use default optimal range for const context
        float distance = 25.0f - 2.0f; // Slightly closer than max (optimal range is 25.0f)

        Position pos;
        pos.m_positionX = this->GetBot()->GetPositionX() + cos(angle) * distance;
        pos.m_positionY = this->GetBot()->GetPositionY() + sin(angle) * distance;
        pos.m_positionZ = this->GetBot()->GetPositionZ();
        pos.SetOrientation(angle);

        return pos;
    }

    /**
     * Check if we should kite
     */
    bool ShouldKite(::Unit* target) const
    {
        return target &&
               target->GetVictim() == this->GetBot() &&
               target->GetDistance(this->GetBot()) < _minimumRange;
    }

private:
    float _kiteDistance;
    float _minimumRange;
};

/**
 * Tank Specialization Template
 * Provides tank-specific defaults and behavior
 */
template<typename ResourceType>
    requires ValidResource<ResourceType>
class TankSpecialization : public CombatSpecializationTemplate<ResourceType>
{
public:
    explicit TankSpecialization(Player* bot)
        : CombatSpecializationTemplate<ResourceType>(bot)
        , _lastTauntTime(0)
        , _defensiveCooldownActive(false)
    {
    }

    float GetOptimalRange(::Unit* target) override final
    {
        return 5.0f; // Melee range for tanks
    }

protected:
    Position GetOptimalPosition(::Unit* target) override
    {
        if (target)
        {
            // Position target facing away from group
            Position groupCenter = CalculateGroupCenter();
            float angleToGroup = target->GetAbsoluteAngle(&groupCenter);
            float optimalAngle = angleToGroup + M_PI; // Opposite of group

            Position pos;
            pos.m_positionX = target->GetPositionX() + cos(optimalAngle) * 3.0f;
            pos.m_positionY = target->GetPositionY() + sin(optimalAngle) * 3.0f;
            pos.m_positionZ = target->GetPositionZ();
            pos.SetOrientation(target->GetAbsoluteAngle(&pos));

            return pos;
        }
        return this->GetBot()->GetPosition();
    }

    /**
     * Manage threat generation
     */
    virtual void ManageThreat(::Unit* target)
    {
        if (!target)
            return;

        // Check if we have aggro
        if (target->GetVictim() != this->GetBot())
        {
            uint32 currentTime = getMSTime();
            if (currentTime - _lastTauntTime > 8000) // 8 second taunt cooldown
            {
                // Use taunt ability (implementation depends on class)
                TauntTarget(target);
                _lastTauntTime = currentTime;
            }
        }
    }

    /**
     * Use defensive cooldowns when needed
     */
    virtual void ManageDefensives()
    {
        float healthPct = this->GetBot()->GetHealthPct();

        if (healthPct < 30.0f && !_defensiveCooldownActive)
        {
            UseDefensiveCooldown();
            _defensiveCooldownActive = true;
        }
        else if (healthPct > 60.0f)
        {
            _defensiveCooldownActive = false;
        }
    }

    /**
     * Calculate center position of group members
     */
    Position CalculateGroupCenter() const
    {
        Position center;
        uint32 count = 0;

        if (Group* group = this->GetBot()->GetGroup())
        {
            for (GroupReference& itr : group->GetMembers())
            {
                if (Player* member = itr.GetSource())
                {
                    if (member != this->GetBot() && member->IsAlive())
                    {
                        center.m_positionX += member->GetPositionX();
                        center.m_positionY += member->GetPositionY();
                        center.m_positionZ += member->GetPositionZ();
                        count++;
                    }
                }
            }
        }

        if (count > 0)
        {
            center.m_positionX /= count;
            center.m_positionY /= count;
            center.m_positionZ /= count;
        }
        else
        {
            center = this->GetBot()->GetPosition();
        }

        return center;
    }

    /**
     * Virtual method for class-specific taunt
     */
    virtual void TauntTarget(::Unit* target) {}

    /**
     * Virtual method for class-specific defensive cooldown
     */
    virtual void UseDefensiveCooldown() {}

private:
    uint32 _lastTauntTime;
    bool _defensiveCooldownActive;
};

/**
 * Healer Specialization Template
 * Provides healer-specific defaults and behavior
 */
template<typename ResourceType>
    requires ValidResource<ResourceType>
class HealerSpecialization : public CombatSpecializationTemplate<ResourceType>
{
public:
    explicit HealerSpecialization(Player* bot)
        : CombatSpecializationTemplate<ResourceType>(bot)
        , _lastGroupHealTime(0)
        , _emergencyHealThreshold(0.3f)
    {
    }

    float GetOptimalRange(::Unit* target) override final
    {
        return 30.0f; // Healing range
    }

protected:
    Position GetOptimalPosition(::Unit* target) override
    {
        // Position at max range from enemies, central to allies
        Position allyCenter = CalculateAllyCenter();
        Position enemyCenter = CalculateEnemyCenter();

        if (enemyCenter.IsPositionValid())
        {
            // Move away from enemies while staying near allies
            float angleFromEnemies = allyCenter.GetRelativeAngle(&enemyCenter) + M_PI;

            Position pos;
            pos.m_positionX = allyCenter.m_positionX + cos(angleFromEnemies) * 15.0f;
            pos.m_positionY = allyCenter.m_positionY + sin(angleFromEnemies) * 15.0f;
            pos.m_positionZ = allyCenter.m_positionZ;
            pos.SetOrientation(angleFromEnemies);

            return pos;
        }

        return allyCenter;
    }

    /**
     * Select best healing target
     */
    virtual Unit* SelectHealTarget()
    {
        Unit* lowestHealthTarget = nullptr;
        float lowestHealthPct = 100.0f;

        // Check self first
        if (this->GetBot()->GetHealthPct() < _emergencyHealThreshold * 100)
        {
            return this->GetBot();
        }

        // Check group members
        if (Group* group = this->GetBot()->GetGroup())
        {
            for (GroupReference& itr : group->GetMembers())
            {
                if (Player* member = itr.GetSource())
                {
                    if (member->IsAlive() && member->GetHealthPct() < lowestHealthPct)
                    {
                        lowestHealthPct = member->GetHealthPct();
                        lowestHealthTarget = member;
                    }
                }
            }
        }

        return lowestHealthTarget;
    }

    /**
     * Check if group needs AoE healing
     */
    bool NeedsGroupHeal() const
    {
        uint32 injuredCount = 0;
        float totalHealthDeficit = 0.0f;

        if (Group* group = this->GetBot()->GetGroup())
        {
            for (GroupReference& itr : group->GetMembers())
            {
                if (Player* member = itr.GetSource())
                {
                    if (member->IsAlive() && member->GetHealthPct() < 80.0f)
                    {
                        injuredCount++;
                        totalHealthDeficit += (100.0f - member->GetHealthPct());
                    }
                }
            }
        }

        // Need group heal if 3+ injured or total deficit > 150%
        return injuredCount >= 3 || totalHealthDeficit > 150.0f;
    }

    /**
     * Calculate center position of allies
     */
    Position CalculateAllyCenter() const
    {
        Position center;
        uint32 count = 0;

        if (Group* group = this->GetBot()->GetGroup())
        {
            for (GroupReference& itr : group->GetMembers())
            {
                if (Player* member = itr.GetSource())
                {
                    if (member->IsAlive())
                    {
                        center.m_positionX += member->GetPositionX();
                        center.m_positionY += member->GetPositionY();
                        center.m_positionZ += member->GetPositionZ();
                        count++;
                    }
                }
            }
        }

        if (count > 0)
        {
            center.m_positionX /= count;
            center.m_positionY /= count;
            center.m_positionZ /= count;
        }
        else
        {
            center = this->GetBot()->GetPosition();
        }

        return center;
    }

    /**
     * Calculate center position of enemies
     */
    Position CalculateEnemyCenter() const
    {
        Position center;
        uint32 count = 0;

        Player* bot = this->GetBot();
        if (!bot)
            return center;

        // Find all hostile units in range
        std::list<Unit*> hostileUnits;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck checker(bot, bot, 40.0f);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, hostileUnits, checker);
        Cell::VisitAllObjects(bot, searcher, 40.0f);

        for (Unit* hostile : hostileUnits)
        {
            center.m_positionX += hostile->GetPositionX();
            center.m_positionY += hostile->GetPositionY();
            center.m_positionZ += hostile->GetPositionZ();
            count++;
        }

        if (count > 0)
        {
            center.m_positionX /= count;
            center.m_positionY /= count;
            center.m_positionZ /= count;
        }

        return center;
    }

private:
    uint32 _lastGroupHealTime;
    float _emergencyHealThreshold;
};

// ============================================================================
// RESOURCE TYPE ALIASES - For easy specialization use
// ============================================================================

// Simple resource types
using ManaResource = uint32;
using RageResource = uint32;
using EnergyResource = uint32;
using FocusResource = uint32;
using RunicPowerResource = uint32;

// Type aliases for common specializations
template<typename T>
using ManaSpecialization = CombatSpecializationTemplate<ManaResource>;

template<typename T>
using RageSpecialization = CombatSpecializationTemplate<RageResource>;

template<typename T>
using EnergySpecialization = CombatSpecializationTemplate<EnergyResource>;

// Role-specific with resource types
using MeleeManaSpec = MeleeDpsSpecialization<ManaResource>;
using MeleeRageSpec = MeleeDpsSpecialization<RageResource>;
using MeleeEnergySpec = MeleeDpsSpecialization<EnergyResource>;

using RangedManaSpec = RangedDpsSpecialization<ManaResource>;
using RangedFocusSpec = RangedDpsSpecialization<FocusResource>;

using TankRageSpec = TankSpecialization<RageResource>;
using TankRunicSpec = TankSpecialization<RunicPowerResource>;

using HealerManaSpec = HealerSpecialization<ManaResource>;

} // namespace Playerbot