/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Phase 3 God Class Refactoring - Mock Object Framework
 *
 * This header provides comprehensive mock implementations of all TrinityCore
 * dependencies required for testing ClassAI specializations in isolation.
 *
 * Design Principles:
 * - Complete interface coverage (all methods mocked)
 * - Configurable behavior (success/failure scenarios)
 * - State tracking (call counts, parameters)
 * - Performance-conscious (no unnecessary allocations)
 * - Thread-safe (for concurrent test execution)
 *
 * Usage:
 *   auto mockPlayer = MockFactory::CreateMockPlayer(CLASS_PRIEST, 80);
 *   auto mockTarget = MockFactory::CreateMockEnemy(70);
 *   EXPECT_CALL(*mockPlayer, CastSpell(_, FLASH_HEAL, _))
 *       .Times(1)
 *       .WillOnce(Return(SPELL_CAST_OK));
 */

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include "Position.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <atomic>

namespace Playerbot
{
namespace Test
{

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

class MockPlayer;
class MockUnit;
class MockSpellInfo;
class MockAura;
class MockGroup;
class MockWorldSession;
class MockBotAI;
class MockMap;

// ============================================================================
// MOCK ENUMERATIONS (Match TrinityCore enums)
// ============================================================================

enum MockSpellCastResult
{
    SPELL_CAST_OK                           = 0,
    SPELL_FAILED_NOT_READY                  = 1,
    SPELL_FAILED_OUT_OF_RANGE               = 2,
    SPELL_FAILED_NOT_ENOUGH_MANA            = 3,
    SPELL_FAILED_CASTER_DEAD                = 4,
    SPELL_FAILED_TARGET_DEAD                = 5,
    SPELL_FAILED_LINE_OF_SIGHT              = 6,
    SPELL_FAILED_BAD_TARGETS                = 7,
};

enum MockPowers
{
    POWER_MANA                              = 0,
    POWER_RAGE                              = 1,
    POWER_FOCUS                             = 2,
    POWER_ENERGY                            = 3,
    POWER_HAPPINESS                         = 4,
    POWER_RUNE                              = 5,
    POWER_RUNIC_POWER                       = 6,
    POWER_MAX                               = 7
};

enum MockClasses
{
    CLASS_NONE                              = 0,
    CLASS_WARRIOR                           = 1,
    CLASS_PALADIN                           = 2,
    CLASS_HUNTER                            = 3,
    CLASS_ROGUE                             = 4,
    CLASS_PRIEST                            = 5,
    CLASS_DEATH_KNIGHT                      = 6,
    CLASS_SHAMAN                            = 7,
    CLASS_MAGE                              = 8,
    CLASS_WARLOCK                           = 9,
    CLASS_DRUID                             = 11
};

// ============================================================================
// MOCK UNIT - Base combat entity
// ============================================================================

class MockUnit
{
public:
    MockUnit();
    virtual ~MockUnit() = default;

    // Core identification
    MOCK_METHOD(ObjectGuid, GetGUID, (), (const));
    MOCK_METHOD(std::string, GetName, (), (const));
    MOCK_METHOD(uint32, GetEntry, (), (const));

    // Health and Power
    MOCK_METHOD(uint32, GetHealth, (), (const));
    MOCK_METHOD(uint32, GetMaxHealth, (), (const));
    MOCK_METHOD(uint32, GetPower, (MockPowers), (const));
    MOCK_METHOD(uint32, GetMaxPower, (MockPowers), (const));
    MOCK_METHOD(float, GetHealthPct, (), (const));
    MOCK_METHOD(float, GetPowerPct, (MockPowers), (const));

    // State queries
    MOCK_METHOD(bool, IsAlive, (), (const));
    MOCK_METHOD(bool, IsDead, (), (const));
    MOCK_METHOD(bool, IsInCombat, (), (const));
    MOCK_METHOD(bool, IsFriendlyTo, (MockUnit const*), (const));
    MOCK_METHOD(bool, IsHostileTo, (MockUnit const*), (const));
    MOCK_METHOD(bool, IsInWorld, (), (const));

    // Position and movement
    MOCK_METHOD(Position, GetPosition, (), (const));
    MOCK_METHOD(float, GetDistance, (MockUnit const*), (const));
    MOCK_METHOD(bool, IsWithinLOSInMap, (MockUnit const*), (const));
    MOCK_METHOD(bool, IsWithinDistInMap, (MockUnit const*, float), (const));

    // Combat targeting
    MOCK_METHOD(MockUnit*, GetVictim, (), (const));
    MOCK_METHOD(void, SetTarget, (ObjectGuid));
    MOCK_METHOD(ObjectGuid, GetTargetGUID, (), (const));

    // Spell casting (pure virtual - must be mocked)
    MOCK_METHOD(MockSpellCastResult, CastSpell, (MockUnit*, uint32, bool), ());
    MOCK_METHOD(bool, IsNonMeleeSpellCast, (bool), (const));
    MOCK_METHOD(void, InterruptNonMeleeSpells, (bool));

    // Aura management
    MOCK_METHOD(bool, HasAura, (uint32), (const));
    MOCK_METHOD(MockAura*, GetAura, (uint32), (const));
    MOCK_METHOD(void, AddAura, (uint32, MockUnit*), ());
    MOCK_METHOD(void, RemoveAura, (uint32), ());

    // Cooldown tracking
    MOCK_METHOD(bool, HasSpellCooldown, (uint32), (const));
    MOCK_METHOD(uint32, GetSpellCooldownDelay, (uint32), (const));
    MOCK_METHOD(void, AddSpellCooldown, (uint32, uint32, uint32), ());

    // Helper methods (not mocked, implemented)
    void SetHealth(uint32 health) { m_health = health; }
    void SetMaxHealth(uint32 maxHealth) { m_maxHealth = maxHealth; }
    void SetPower(MockPowers power, uint32 value) { m_power[power] = value; }
    void SetMaxPower(MockPowers power, uint32 value) { m_maxPower[power] = value; }
    void SetPosition(Position pos) { m_position = pos; }
    void SetCombatState(bool inCombat) { m_inCombat = inCombat; }

protected:
    ObjectGuid m_guid;
    std::string m_name;
    uint32 m_entry = 0;
    uint32 m_health = 1;
    uint32 m_maxHealth = 1;
    std::unordered_map<MockPowers, uint32> m_power;
    std::unordered_map<MockPowers, uint32> m_maxPower;
    Position m_position;
    bool m_inCombat = false;
    bool m_alive = true;
    ObjectGuid m_targetGuid;
};

// ============================================================================
// MOCK PLAYER - Extends MockUnit with player-specific functionality
// ============================================================================

class MockPlayer : public MockUnit
{
public:
    MockPlayer();
    ~MockPlayer() override = default;

    // Player identification
    MOCK_METHOD(uint8, GetClass, (), (const));
    MOCK_METHOD(uint8, GetRace, (), (const));
    MOCK_METHOD(uint8, GetLevel, (), (const));
    MOCK_METHOD(uint32, GetSpec, (), (const));

    // Group and social
    MOCK_METHOD(bool, IsInGroup, (), (const));
    MOCK_METHOD(MockGroup*, GetGroup, (), (const));
    MOCK_METHOD(bool, IsInRaid, (), (const));
    MOCK_METHOD(bool, IsGroupLeader, (), (const));

    // Equipment and inventory
    MOCK_METHOD(uint32, GetItemCount, (uint32, bool), (const));
    MOCK_METHOD(bool, HasItemInInventory, (uint32), (const));

    // Spells and talents
    MOCK_METHOD(bool, HasSpell, (uint32), (const));
    MOCK_METHOD(bool, HasTalent, (uint32, uint8), (const));
    MOCK_METHOD(uint32, GetTalentRank, (uint32), (const));

    // AI and bot control
    MOCK_METHOD(MockBotAI*, GetPlayerbotAI, (), (const));
    MOCK_METHOD(MockWorldSession*, GetSession, (), (const));
    MOCK_METHOD(bool, IsBot, (), (const));

    // Helper configuration methods
    void SetClass(uint8 classId) { m_class = classId; }
    void SetLevel(uint8 level) { m_level = level; }
    void SetSpec(uint32 spec) { m_spec = spec; }
    void AddSpell(uint32 spellId) { m_spells.insert(spellId); }
    void SetGroup(MockGroup* group) { m_group = group; }

private:
    uint8 m_class = CLASS_NONE;
    uint8 m_race = 1; // Human
    uint8 m_level = 1;
    uint32 m_spec = 0;
    std::unordered_set<uint32> m_spells;
    MockGroup* m_group = nullptr;
};

// ============================================================================
// MOCK SPELL INFO - Spell data and properties
// ============================================================================

class MockSpellInfo
{
public:
    MockSpellInfo(uint32 spellId);

    MOCK_METHOD(uint32, GetId, (), (const));
    MOCK_METHOD(std::string, GetName, (), (const));
    MOCK_METHOD(uint32, GetManaCost, (), (const));
    MOCK_METHOD(uint32, GetManaCostPercentage, (), (const));
    MOCK_METHOD(uint32, GetCooldown, (), (const));
    MOCK_METHOD(uint32, GetRecoveryTime, (), (const));
    MOCK_METHOD(uint32, GetCategoryRecoveryTime, (), (const));
    MOCK_METHOD(uint32, GetCastTime, (), (const));
    MOCK_METHOD(float, GetMinRange, (), (const));
    MOCK_METHOD(float, GetMaxRange, (), (const));
    MOCK_METHOD(uint32, GetSchoolMask, (), (const));
    MOCK_METHOD(uint32, GetSpellLevel, (), (const));
    MOCK_METHOD(uint32, GetMaxLevel, (), (const));
    MOCK_METHOD(bool, IsChanneled, (), (const));
    MOCK_METHOD(bool, IsPositive, (), (const));
    MOCK_METHOD(bool, IsPassive, (), (const));

    // Configuration helpers
    void SetManaCost(uint32 cost) { m_manaCost = cost; }
    void SetCooldown(uint32 cooldown) { m_cooldown = cooldown; }
    void SetCastTime(uint32 castTime) { m_castTime = castTime; }
    void SetRange(float min, float max) { m_minRange = min; m_maxRange = max; }

private:
    uint32 m_spellId;
    uint32 m_manaCost = 0;
    uint32 m_cooldown = 0;
    uint32 m_castTime = 0;
    float m_minRange = 0.0f;
    float m_maxRange = 40.0f;
};

// ============================================================================
// MOCK AURA - Buff/debuff tracking
// ============================================================================

class MockAura
{
public:
    MockAura(uint32 spellId, MockUnit* caster, MockUnit* target);

    MOCK_METHOD(uint32, GetId, (), (const));
    MOCK_METHOD(MockUnit*, GetCaster, (), (const));
    MOCK_METHOD(MockUnit*, GetTarget, (), (const));
    MOCK_METHOD(uint32, GetDuration, (), (const));
    MOCK_METHOD(uint32, GetMaxDuration, (), (const));
    MOCK_METHOD(uint8, GetStackAmount, (), (const));
    MOCK_METHOD(bool, IsPositive, (), (const));
    MOCK_METHOD(void, SetDuration, (uint32));
    MOCK_METHOD(void, RefreshDuration, ());
    MOCK_METHOD(void, Remove, ());

    void SetStackAmount(uint8 stacks) { m_stacks = stacks; }

private:
    uint32 m_spellId;
    MockUnit* m_caster;
    MockUnit* m_target;
    uint32 m_duration = 0;
    uint32 m_maxDuration = 0;
    uint8 m_stacks = 1;
};

// ============================================================================
// MOCK GROUP - Party/raid functionality
// ============================================================================

class MockGroup
{
public:
    MockGroup();

    MOCK_METHOD(ObjectGuid, GetGUID, (), (const));
    MOCK_METHOD(ObjectGuid, GetLeaderGUID, (), (const));
    MOCK_METHOD(MockPlayer*, GetLeader, (), (const));
    MOCK_METHOD(uint32, GetMembersCount, (), (const));
    MOCK_METHOD(bool, IsMember, (ObjectGuid), (const));
    MOCK_METHOD(bool, IsLeader, (ObjectGuid), (const));
    MOCK_METHOD(bool, IsAssistant, (ObjectGuid), (const));
    MOCK_METHOD(bool, AddMember, (MockPlayer*), ());
    MOCK_METHOD(bool, RemoveMember, (ObjectGuid), ());
    MOCK_METHOD(void, SetLeader, (ObjectGuid));

    // Helper methods
    void AddMemberHelper(MockPlayer* player) { m_members.push_back(player); }
    std::vector<MockPlayer*> const& GetMembers() const { return m_members; }

private:
    ObjectGuid m_guid;
    ObjectGuid m_leaderGuid;
    std::vector<MockPlayer*> m_members;
};

// ============================================================================
// MOCK WORLD SESSION - Network session representation
// ============================================================================

class MockWorldSession
{
public:
    MockWorldSession();

    MOCK_METHOD(MockPlayer*, GetPlayer, (), (const));
    MOCK_METHOD(bool, IsBot, (), (const));
    MOCK_METHOD(uint32, GetAccountId, (), (const));
    MOCK_METHOD(std::string, GetPlayerName, (), (const));

    void SetPlayer(MockPlayer* player) { m_player = player; }
    void SetIsBot(bool isBot) { m_isBot = isBot; }

private:
    MockPlayer* m_player = nullptr;
    bool m_isBot = false;
};

// ============================================================================
// MOCK BOT AI - Minimal BotAI interface for ClassAI testing
// ============================================================================

class MockBotAI
{
public:
    MockBotAI();

    MOCK_METHOD(MockPlayer*, GetBot, (), (const));
    MOCK_METHOD(bool, IsActive, (), (const));
    MOCK_METHOD(bool, IsInCombat, (), (const));
    MOCK_METHOD(ObjectGuid, GetTarget, (), (const));
    MOCK_METHOD(void, SetTarget, (ObjectGuid));

    // Value system (for shared bot values)
    MOCK_METHOD(float, GetValue, (std::string const&), (const));
    MOCK_METHOD(void, SetValue, (std::string const&, float));

    void SetBot(MockPlayer* bot) { m_bot = bot; }

private:
    MockPlayer* m_bot = nullptr;
};

// ============================================================================
// MOCK FACTORY - Centralized mock object creation
// ============================================================================

class MockFactory
{
public:
    // Player creation
    static std::shared_ptr<MockPlayer> CreateMockPlayer(
        uint8 classId = CLASS_PRIEST,
        uint8 level = 80,
        uint32 spec = 0
    );

    // Enemy creation
    static std::shared_ptr<MockUnit> CreateMockEnemy(
        uint32 level = 80,
        uint32 health = 10000
    );

    // Friendly NPC creation
    static std::shared_ptr<MockUnit> CreateMockAlly(
        uint32 level = 80,
        uint32 health = 10000
    );

    // Spell info creation
    static std::shared_ptr<MockSpellInfo> CreateMockSpellInfo(
        uint32 spellId,
        uint32 manaCost = 100,
        uint32 cooldown = 0,
        uint32 castTime = 1500
    );

    // Aura creation
    static std::shared_ptr<MockAura> CreateMockAura(
        uint32 spellId,
        MockUnit* caster,
        MockUnit* target,
        uint32 duration = 30000
    );

    // Group creation
    static std::shared_ptr<MockGroup> CreateMockGroup(
        MockPlayer* leader
    );

    // Session creation
    static std::shared_ptr<MockWorldSession> CreateMockSession(
        MockPlayer* player,
        bool isBot = true
    );

    // BotAI creation
    static std::shared_ptr<MockBotAI> CreateMockBotAI(
        MockPlayer* bot
    );

    // Scenario builders
    static struct CombatScenario CreateCombatScenario(
        uint8 classId,
        uint8 level,
        uint32 enemyCount = 1
    );

    static struct HealingScenario CreateHealingScenario(
        uint8 healerClass,
        uint32 groupSize = 5,
        float avgHealthPct = 50.0f
    );

    static struct GroupScenario CreateGroupScenario(
        uint32 groupSize = 5,
        bool hasHealer = true,
        bool hasTank = true
    );

private:
    static std::atomic<uint64> s_guidCounter;
    static ObjectGuid GenerateGUID();
};

// ============================================================================
// SCENARIO STRUCTURES - Pre-configured test scenarios
// ============================================================================

struct CombatScenario
{
    std::shared_ptr<MockPlayer> player;
    std::shared_ptr<MockBotAI> botAI;
    std::vector<std::shared_ptr<MockUnit>> enemies;
    std::shared_ptr<MockGroup> group; // Optional
};

struct HealingScenario
{
    std::shared_ptr<MockPlayer> healer;
    std::shared_ptr<MockBotAI> healerAI;
    std::shared_ptr<MockGroup> group;
    std::vector<std::shared_ptr<MockPlayer>> groupMembers;
};

struct GroupScenario
{
    std::shared_ptr<MockGroup> group;
    std::shared_ptr<MockPlayer> tank;
    std::shared_ptr<MockPlayer> healer;
    std::vector<std::shared_ptr<MockPlayer>> dps;
    std::shared_ptr<MockUnit> boss;
};

// ============================================================================
// TEST ASSERTION HELPERS
// ============================================================================

// Spell cast assertions
#define EXPECT_SPELL_CAST(player, spellId) \
    EXPECT_CALL(*player, CastSpell(::testing::_, spellId, ::testing::_)) \
        .Times(::testing::AtLeast(1))

#define EXPECT_SPELL_NOT_CAST(player, spellId) \
    EXPECT_CALL(*player, CastSpell(::testing::_, spellId, ::testing::_)) \
        .Times(0)

#define EXPECT_SPELL_CAST_ON_TARGET(player, spellId, target) \
    EXPECT_CALL(*player, CastSpell(target.get(), spellId, ::testing::_)) \
        .Times(::testing::AtLeast(1))

// Resource assertions
#define EXPECT_MANA_SUFFICIENT(player, spellCost) \
    EXPECT_GE(player->GetPower(POWER_MANA), spellCost)

#define EXPECT_COOLDOWN_READY(player, spellId) \
    EXPECT_FALSE(player->HasSpellCooldown(spellId))

// Combat state assertions
#define EXPECT_IN_COMBAT(unit) \
    EXPECT_TRUE(unit->IsInCombat())

#define EXPECT_NOT_IN_COMBAT(unit) \
    EXPECT_FALSE(unit->IsInCombat())

#define EXPECT_TARGET_SET(unit, expectedTarget) \
    EXPECT_EQ(unit->GetTargetGUID(), expectedTarget->GetGUID())

// Performance assertions
#define EXPECT_EXECUTION_TIME_UNDER_MICROS(operation, limitMicros) \
    { \
        auto start = std::chrono::high_resolution_clock::now(); \
        operation; \
        auto end = std::chrono::high_resolution_clock::now(); \
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(); \
        EXPECT_LE(duration, limitMicros) << "Operation took " << duration << "µs, expected <" << limitMicros << "µs"; \
    }

} // namespace Test
} // namespace Playerbot
