/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 */

#include "CrowdControlManager.h"
#include "Player.h"
#include "Unit.h"
#include "Group.h"
#include "ThreatManager.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "Log.h"
#include "Creature.h"
#include "GameTime.h"
#include "DBCEnums.h"  // For MAX_EFFECT_MASK
#include <algorithm>

namespace Playerbot
{

CrowdControlManager::CrowdControlManager(Player* bot)
    : _bot(bot)
    , _lastUpdate(0)
{
}

void CrowdControlManager::Update(uint32 diff, const CombatMetrics& metrics)
{
    if (!_bot)
        return;

    _lastUpdate += diff;

    if (_lastUpdate < UPDATE_INTERVAL)
        return;

    _lastUpdate = 0;

    // Update expired CCs
    UpdateExpiredCCs();
}

void CrowdControlManager::Reset()
{
    _activeCCs.clear();
    _lastUpdate = 0;
}

bool CrowdControlManager::ShouldUseCrowdControl()
{
    if (!_bot)
        return false;

    // Get combat enemies
    ::std::vector<Unit*> enemies = GetCombatEnemies();

    // Need at least 2 enemies to justify CC
    if (enemies.size() < 2)
        return false;

    // Check for uncrowded targets
    uint32 uncrowdedCount = 0;
    for (Unit* enemy : enemies)
    {
        if (!IsTargetCCd(enemy))
            ++uncrowdedCount;
    }

    // Need at least 1 uncrowded target
    if (uncrowdedCount == 0)
        return false;

    // Check if bot has CC abilities available
    ::std::vector<uint32> ccSpells = GetAvailableCCSpells();
    return !ccSpells.empty();
}

Unit* CrowdControlManager::GetPriorityTarget()
{
    if (!_bot)
        return nullptr;

    ::std::vector<Unit*> enemies = GetCombatEnemies();

    // Score each target
    ::std::vector<::std::pair<Unit*, float>> scored;

    for (Unit* enemy : enemies)
    {
        // Skip already CC'd targets
    if (IsTargetCCd(enemy))
            continue;

        float priority = CalculateCCPriority(enemy);
        if (priority > 0.0f)
            scored.emplace_back(enemy, priority);
    }

    if (scored.empty())
        return nullptr;

    // Sort by priority (highest first)
    ::std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) {
            return a.second > b.second;
        });

    return scored.front().first;
}

uint32 CrowdControlManager::GetRecommendedSpell(Unit* target)
{
    if (!target)
        return 0;

    ::std::vector<uint32> spells = GetAvailableCCSpells();

    // Filter spells suitable for target
    for (uint32 spellId : spells)
    {
        if (IsSpellSuitableForTarget(spellId, target))
            return spellId;
    }

    return 0;
}

bool CrowdControlManager::ShouldCC(Unit* target, CrowdControlType type)
{
    if (!target || !_bot)
        return false;

    // Already CC'd?
    if (IsTargetCCd(target))
        return false;

    // Immune?
    if (IsImmune(target, type))
        return false;

    // Valid threat?
    if (target->isDead() || target->IsFriendlyTo(_bot))
        return false;

    return true;
}

void CrowdControlManager::ApplyCC(Unit* target, CrowdControlType type, uint32 duration, Player* bot, uint32 spellId)
{
    if (!target)
        return;

    CCTarget cc;
    cc.target = target;
    cc.type = type;
    cc.duration = duration;
    cc.appliedBy = bot;
    cc.expiryTime = GameTime::GetGameTimeMS() + duration;
    cc.spellId = spellId;

    _activeCCs[target->GetGUID()] = cc;

    TC_LOG_DEBUG("playerbot", "CrowdControlManager: {} applied {} CC on {} for {}ms",
        bot ? bot->GetName() : "unknown",
        static_cast<uint32>(type),
        target->GetName(),
        duration);
}

void CrowdControlManager::RemoveCC(Unit* target)
{
    if (!target)
        return;

    _activeCCs.erase(target->GetGUID());
}

Player* CrowdControlManager::GetChainCCBot(Unit* target)
{
    if (!target || !_bot)
        return nullptr;

    // Check if target needs chain CC soon
    const CCTarget* cc = GetActiveCC(target);
    if (!cc)
        return nullptr;

    // Only chain if within window
    if (cc->GetRemainingTime() > CHAIN_CC_WINDOW)
        return nullptr;

    // In group? Assign to another bot
    Group* group = _bot->GetGroup();
    if (group)
    {
        // Find another player with CC
    for (GroupReference& ref : group->GetMembers())
        {
            Player* member = ref.GetSource();
            if (!member || member == cc->appliedBy)
                continue;

            // TODO: Check if member has CC available
            return member;
        }
    }

    // Solo or no other CCer: bot reapplies
    return _bot;
}

bool CrowdControlManager::IsTargetCCd(Unit* target) const
{
    if (!target)
        return false;

    return GetActiveCC(target) != nullptr;
}

const CCTarget* CrowdControlManager::GetActiveCC(Unit* target) const
{
    if (!target)
        return nullptr;

    auto it = _activeCCs.find(target->GetGUID());
    if (it == _activeCCs.end())
        return nullptr;

    // Check if still active
    if (!it->second.IsActive())
        return nullptr;

    return &it->second;
}

::std::vector<Unit*> CrowdControlManager::GetCCdTargets() const
{
    ::std::vector<Unit*> targets;

    for (const auto& [guid, cc] : _activeCCs)
    {
        if (cc.IsActive() && cc.target)
            targets.push_back(cc.target);
    }

    return targets;
}

bool CrowdControlManager::ShouldBreakCC(Unit* target) const
{
    if (!target)
        return false;

    // Check if last enemy
    ::std::vector<Unit*> enemies = GetCombatEnemies();

    uint32 activeEnemies = 0;
    for (Unit* enemy : enemies)
    {
        if (enemy && !enemy->isDead() && !IsTargetCCd(enemy))
            ++activeEnemies;
    }

    // If only CC'd targets left, safe to break
    return activeEnemies == 0;
}

// Private helper functions

::std::vector<Unit*> CrowdControlManager::GetCombatEnemies() const
{
    ::std::vector<Unit*> enemies;

    if (!_bot)
        return enemies;

    ThreatManager& threatMgr = _bot->GetThreatManager();

    for (ThreatReference const* ref : threatMgr.GetUnsortedThreatList())
    {
        if (!ref)
            continue;

        Unit* enemy = ref->GetVictim();
        if (enemy && !enemy->isDead())
            enemies.push_back(enemy);
    }

    return enemies;
}

bool CrowdControlManager::IsImmune(Unit* target, CrowdControlType type) const
{
    if (!target)
        return true;

    // Check for CC immunity based on type
    switch (type)
    {
        case CrowdControlType::STUN:
            return target->HasAuraType(SPELL_AURA_MECHANIC_IMMUNITY);
        case CrowdControlType::INCAPACITATE:
            return target->HasAuraType(SPELL_AURA_MECHANIC_IMMUNITY);
        case CrowdControlType::DISORIENT:
            return target->HasAuraType(SPELL_AURA_MECHANIC_IMMUNITY);
        case CrowdControlType::ROOT:
            return target->HasAuraType(SPELL_AURA_MECHANIC_IMMUNITY);
        case CrowdControlType::SILENCE:
            return target->HasAuraType(SPELL_AURA_MOD_SILENCE);
        default:
            return false;
    }
}

float CrowdControlManager::CalculateCCPriority(Unit* target) const
{
    if (!target || !_bot)
        return 0.0f;

    float priority = 10.0f;  // Base priority

    // Healer = highest priority
    if (target->GetPowerType() == POWER_MANA)
    {
        // Simple heuristic: mana users are likely healers/casters
        priority += 40.0f;
    }

    // Caster bonus
    if (target->GetPowerType() == POWER_MANA)
        priority += 20.0f;

    // High HP = higher priority (will take longer to kill)
    if (target->GetHealthPct() > 80.0f)
        priority += 15.0f;

    // Elite bonus
    if (Creature* creature = target->ToCreature())
    {
        if (creature->IsElite())
            priority += 10.0f;
    }

    // Distance penalty (prefer nearby targets)
    float distance = _bot->GetDistance(target);
    if (distance > 30.0f)
        priority *= 0.7f;

    return priority;
}

::std::vector<uint32> CrowdControlManager::GetAvailableCCSpells() const
{
    ::std::vector<uint32> spells;

    if (!_bot)
        return spells;

    // Class-specific CC spell database
    // Maps class to list of CC spell IDs
    struct CCSpellInfo
    {
        uint32 spellId;
        CrowdControlType ccType;
    };

    static const std::unordered_map<uint8, std::vector<CCSpellInfo>> classCCSpells = {
        // Mage
        {CLASS_MAGE, {
            {118, CrowdControlType::INCAPACITATE},      // Polymorph
            {82691, CrowdControlType::INCAPACITATE},    // Ring of Frost
            {122, CrowdControlType::ROOT},              // Frost Nova
            {31661, CrowdControlType::DISORIENT},       // Dragon's Breath
        }},
        // Rogue
        {CLASS_ROGUE, {
            {6770, CrowdControlType::INCAPACITATE},     // Sap
            {1776, CrowdControlType::STUN},             // Gouge
            {2094, CrowdControlType::DISORIENT},        // Blind
            {408, CrowdControlType::STUN},              // Kidney Shot
        }},
        // Hunter
        {CLASS_HUNTER, {
            {187650, CrowdControlType::INCAPACITATE},   // Freezing Trap
            {19386, CrowdControlType::INCAPACITATE},    // Wyvern Sting
            {213691, CrowdControlType::INCAPACITATE},   // Scatter Shot
            {109248, CrowdControlType::STUN},           // Binding Shot
        }},
        // Warlock
        {CLASS_WARLOCK, {
            {5782, CrowdControlType::DISORIENT},        // Fear
            {710, CrowdControlType::INCAPACITATE},      // Banish
            {6789, CrowdControlType::DISORIENT},        // Mortal Coil
            {30283, CrowdControlType::STUN},            // Shadowfury
        }},
        // Priest
        {CLASS_PRIEST, {
            {9484, CrowdControlType::INCAPACITATE},     // Shackle Undead
            {605, CrowdControlType::INCAPACITATE},      // Mind Control
            {8122, CrowdControlType::DISORIENT},        // Psychic Scream
            {200196, CrowdControlType::STUN},           // Holy Word: Chastise
        }},
        // Druid
        {CLASS_DRUID, {
            {339, CrowdControlType::ROOT},              // Entangling Roots
            {2637, CrowdControlType::INCAPACITATE},     // Hibernate
            {99, CrowdControlType::DISORIENT},          // Incapacitating Roar
            {5211, CrowdControlType::STUN},             // Mighty Bash
            {102359, CrowdControlType::ROOT},           // Mass Entanglement
        }},
        // Shaman
        {CLASS_SHAMAN, {
            {51514, CrowdControlType::INCAPACITATE},    // Hex
            {118905, CrowdControlType::STUN},           // Static Charge
            {197214, CrowdControlType::STUN},           // Sundering
        }},
        // Paladin
        {CLASS_PALADIN, {
            {20066, CrowdControlType::INCAPACITATE},    // Repentance
            {853, CrowdControlType::STUN},              // Hammer of Justice
            {115750, CrowdControlType::STUN},           // Blinding Light
            {10326, CrowdControlType::DISORIENT},       // Turn Evil
        }},
        // Death Knight
        {CLASS_DEATH_KNIGHT, {
            {108194, CrowdControlType::STUN},           // Asphyxiate
            {91807, CrowdControlType::STUN},            // Shambling Rush (Ghoul)
            {207167, CrowdControlType::DISORIENT},      // Blinding Sleet
        }},
        // Monk
        {CLASS_MONK, {
            {115078, CrowdControlType::INCAPACITATE},   // Paralysis
            {119381, CrowdControlType::STUN},           // Leg Sweep
            {198909, CrowdControlType::DISORIENT},      // Song of Chi-Ji
        }},
        // Warrior
        {CLASS_WARRIOR, {
            {5246, CrowdControlType::DISORIENT},        // Intimidating Shout
            {132168, CrowdControlType::STUN},           // Shockwave
            {132169, CrowdControlType::STUN},           // Storm Bolt
        }},
        // Demon Hunter
        {CLASS_DEMON_HUNTER, {
            {217832, CrowdControlType::INCAPACITATE},   // Imprison
            {179057, CrowdControlType::STUN},           // Chaos Nova
            {211881, CrowdControlType::STUN},           // Fel Eruption
        }},
        // Evoker
        {CLASS_EVOKER, {
            {360806, CrowdControlType::INCAPACITATE},   // Sleep Walk
            {357210, CrowdControlType::ROOT},           // Deep Breath knockback
        }},
    };

    // Get spells for bot's class
    uint8 botClass = _bot->GetClass();
    auto classIt = classCCSpells.find(botClass);
    if (classIt == classCCSpells.end())
        return spells;

    // Check each CC spell for availability
    for (const CCSpellInfo& ccInfo : classIt->second)
    {
        // Check if bot knows the spell
        if (!_bot->HasSpell(ccInfo.spellId))
            continue;

        // Check if spell is on cooldown
        if (_bot->GetSpellHistory()->HasCooldown(ccInfo.spellId))
            continue;

        // Check if bot has enough power to cast
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(ccInfo.spellId, DIFFICULTY_NONE);
        if (!spellInfo)
            continue;

        std::vector<SpellPowerCost> costs = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
        bool hasPower = true;
        for (SpellPowerCost const& cost : costs)
        {
            if (_bot->GetPower(cost.Power) < cost.Amount)
            {
                hasPower = false;
                break;
            }
        }

        if (hasPower)
            spells.push_back(ccInfo.spellId);
    }

    return spells;
}

bool CrowdControlManager::IsSpellSuitableForTarget(uint32 spellId, Unit* target) const
{
    if (!target || !_bot || spellId == 0)
        return false;

    // Get spell info
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    // Check if bot knows the spell
    if (!_bot->HasSpell(spellId))
        return false;

    // Check cooldown
    if (_bot->GetSpellHistory()->HasCooldown(spellId))
        return false;

    // Check power cost
    std::vector<SpellPowerCost> costs = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    for (SpellPowerCost const& cost : costs)
    {
        if (_bot->GetPower(cost.Power) < cost.Amount)
            return false;
    }

    // Check range
    float range = spellInfo->GetMaxRange(false);
    if (_bot->GetDistance(target) > range)
        return false;

    // Check line of sight
    if (!_bot->IsWithinLOSInMap(target))
        return false;

    // Check target type based on creature type
    if (Creature* creature = target->ToCreature())
    {
        uint32 creatureType = creature->GetCreatureType();

        // Check spell mechanic/effect to determine which creature types it works on
        // Polymorph-like spells: work on beasts, humanoids, critters
        if (spellInfo->Mechanic == MECHANIC_POLYMORPH ||
            spellInfo->HasAura(SPELL_AURA_MOD_CONFUSE))
        {
            if (creatureType != CREATURE_TYPE_BEAST &&
                creatureType != CREATURE_TYPE_HUMANOID &&
                creatureType != CREATURE_TYPE_CRITTER)
            {
                return false;
            }
        }

        // Banish: works on demons and elementals
        if (spellInfo->Mechanic == MECHANIC_BANISH)
        {
            if (creatureType != CREATURE_TYPE_DEMON &&
                creatureType != CREATURE_TYPE_ELEMENTAL)
            {
                return false;
            }
        }

        // Shackle: works on undead
        if (spellInfo->Mechanic == MECHANIC_SHACKLE ||
            spellInfo->HasAura(SPELL_AURA_MOD_SHAPESHIFT))
        {
            if (creatureType != CREATURE_TYPE_UNDEAD)
            {
                return false;
            }
        }

        // Fear: works on humanoids and beasts (generally)
        if (spellInfo->Mechanic == MECHANIC_FEAR)
        {
            if (creatureType == CREATURE_TYPE_MECHANICAL ||
                creatureType == CREATURE_TYPE_UNDEAD ||
                creatureType == CREATURE_TYPE_ELEMENTAL)
            {
                return false;
            }
        }
    }

    // Check if target is immune to CC
    // Pass MAX_EFFECT_MASK to check all effects of the spell (API changed in 11.2.7)
    if (target->IsImmunedToSpell(spellInfo, MAX_EFFECT_MASK, _bot))
        return false;

    return true;
}

void CrowdControlManager::UpdateExpiredCCs()
{
    uint32 now = GameTime::GetGameTimeMS();

    for (auto it = _activeCCs.begin(); it != _activeCCs.end();)
    {
        if (!it->second.IsActive() || !it->second.target || it->second.target->isDead())
        {
            TC_LOG_DEBUG("playerbot", "CrowdControlManager: CC on {} expired",
                it->second.target ? it->second.target->GetName() : "unknown");
            it = _activeCCs.erase(it);
        }
        else
            ++it;
    }
}

} // namespace Playerbot
