/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Position.h"
#include <vector>
#include <queue>

// Forward declarations
class Player;
class Unit;

namespace Playerbot
{

// Priest specializations
enum class PriestSpec : uint8
{
    DISCIPLINE = 0,
    HOLY = 1,
    SHADOW = 2
};

// Priest role in group
enum class PriestRole : uint8
{
    HEALER = 0,
    DPS = 1,
    HYBRID = 2
};

// Healing priority levels
enum class HealPriority : uint8
{
    EMERGENCY = 0,      // <20% health, imminent death
    CRITICAL = 1,       // 20-40% health, needs immediate attention
    MODERATE = 2,       // 40-70% health, should heal soon
    MAINTENANCE = 3,    // 70-90% health, top off when convenient
    FULL = 4           // >90% health, no healing needed
};

// Heal target info for priority queue
struct HealTarget
{
    ::Unit* target;
    HealPriority priority;
    float healthPercent;
    uint32 missingHealth;
    bool inCombat;
    bool hasHoTs;
    uint32 timestamp;
    float threatLevel;

    HealTarget() : target(nullptr), priority(HealPriority::FULL), healthPercent(100.0f),
                   missingHealth(0), inCombat(false), hasHoTs(false), timestamp(0), threatLevel(0.0f) {}

    HealTarget(::Unit* t, HealPriority p, float hp, uint32 missing)
        : target(t), priority(p), healthPercent(hp), missingHealth(missing),
          inCombat(t ? t->IsInCombat() : false), hasHoTs(false), timestamp(getMSTime()), threatLevel(0.0f) {}

    // Priority comparison for healing queue
    bool operator<(const HealTarget& other) const
    {
        if (priority != other.priority)
            return priority > other.priority; // Lower enum value = higher priority

        if (healthPercent != other.healthPercent)
            return healthPercent > other.healthPercent; // Lower health = higher priority

        return timestamp > other.timestamp; // Older requests get priority
    }
};

// Base interface for priest specializations
class TC_GAME_API PriestSpecialization
{
public:
    explicit PriestSpecialization(Player* bot);
    virtual ~PriestSpecialization() = default;

    // Core specialization interface
    virtual void UpdateRotation(::Unit* target) = 0;
    virtual void UpdateBuffs() = 0;
    virtual void UpdateCooldowns(uint32 diff) = 0;
    virtual bool CanUseAbility(uint32 spellId) = 0;

    // Combat callbacks
    virtual void OnCombatStart(::Unit* target) = 0;
    virtual void OnCombatEnd() = 0;

    // Resource management
    virtual bool HasEnoughResource(uint32 spellId) = 0;
    virtual void ConsumeResource(uint32 spellId) = 0;

    // Positioning
    virtual Position GetOptimalPosition(::Unit* target) = 0;
    virtual float GetOptimalRange(::Unit* target) = 0;

    // Healing interface
    virtual void UpdateHealing() = 0;
    virtual bool ShouldHeal() = 0;
    virtual ::Unit* GetBestHealTarget() = 0;
    virtual void HealTarget(::Unit* target) = 0;

    // Role management
    virtual PriestRole GetCurrentRole() = 0;
    virtual void SetRole(PriestRole role) = 0;

    // Specialization info
    virtual PriestSpec GetSpecialization() const = 0;
    virtual const char* GetSpecializationName() const = 0;

protected:
    Player* _bot;

    // Shared priest utilities
    bool HasEnoughMana(uint32 amount);
    uint32 GetMana();
    uint32 GetMaxMana();
    float GetManaPercent();
    bool ShouldConserveMana();

    // Shared healing abilities
    void CastHeal(::Unit* target);
    void CastGreaterHeal(::Unit* target);
    void CastFlashHeal(::Unit* target);
    void CastRenew(::Unit* target);
    void CastPrayerOfHealing();
    void CastCircleOfHealing();

    // Shared defensive abilities
    void UseFade();
    void UseDispelMagic(::Unit* target);
    void UseFearWard(::Unit* target);
    void UseShieldSpell(::Unit* target);

    // Shared offensive abilities
    void CastMindBlast(::Unit* target);
    void CastShadowWordPain(::Unit* target);
    void CastShadowWordDeath(::Unit* target);
    void CastHolyFire(::Unit* target);
    void CastSmite(::Unit* target);

    // Shared utility
    bool IsChanneling();
    bool IsCasting();
    bool CanCastSpell();
    bool IsInDanger();
    std::vector<::Unit*> GetGroupMembers();
    std::vector<::Unit*> GetInjuredGroupMembers(float healthThreshold = 80.0f);

    // Common constants
    static constexpr float OPTIMAL_HEALING_RANGE = 40.0f;
    static constexpr float OPTIMAL_DPS_RANGE = 30.0f;
    static constexpr float MINIMUM_SAFE_RANGE = 15.0f;
    static constexpr float MANA_CONSERVATION_THRESHOLD = 0.3f;
    static constexpr float MANA_EMERGENCY_THRESHOLD = 0.15f;

    // Common spell IDs
    enum CommonSpells
    {
        // Healing spells
        HEAL = 2050,
        GREATER_HEAL = 2060,
        FLASH_HEAL = 2061,
        RENEW = 139,
        PRAYER_OF_HEALING = 596,
        CIRCLE_OF_HEALING = 34861,
        BINDING_HEAL = 32546,

        // Defensive spells
        FADE = 586,
        DISPEL_MAGIC = 527,
        FEAR_WARD = 6346,
        POWER_WORD_SHIELD = 17,
        PRAYER_OF_MENDING = 33076,

        // Offensive spells
        MIND_BLAST = 8092,
        SHADOW_WORD_PAIN = 589,
        SHADOW_WORD_DEATH = 32379,
        HOLY_FIRE = 14914,
        SMITE = 585,

        // Buffs
        POWER_WORD_FORTITUDE = 1243,
        DIVINE_SPIRIT = 14752,
        SHADOW_PROTECTION = 976,
        INNER_FIRE = 588
    };
};

} // namespace Playerbot