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
#include "Log.h"
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
    std::vector<Unit*> enemies = GetCombatEnemies();

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
    std::vector<uint32> ccSpells = GetAvailableCCSpells();
    return !ccSpells.empty();
}

Unit* CrowdControlManager::GetPriorityTarget()
{
    if (!_bot)
        return nullptr;

    std::vector<Unit*> enemies = GetCombatEnemies();

    // Score each target
    std::vector<std::pair<Unit*, float>> scored;

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
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) {
            return a.second > b.second;
        });

    return scored.front().first;
}

uint32 CrowdControlManager::GetRecommendedSpell(Unit* target)
{
    if (!target)
        return 0;

    std::vector<uint32> spells = GetAvailableCCSpells();

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

std::vector<Unit*> CrowdControlManager::GetCCdTargets() const
{
    std::vector<Unit*> targets;

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
    std::vector<Unit*> enemies = GetCombatEnemies();

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

std::vector<Unit*> CrowdControlManager::GetCombatEnemies() const
{
    std::vector<Unit*> enemies;

    if (!_bot)
        return enemies;

    ThreatManager& threatMgr = _bot->GetThreatManager();
    std::list<HostileReference*> const& threatList = threatMgr.GetThreatList();

    for (HostileReference* ref : threatList)
    {
        if (!ref)
            continue;

        Unit* enemy = ref->GetOwner();
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
            return target->IsImmunedToSpellEffect(SPELL_EFFECT_ATTACK_ME);  // Placeholder
        case CrowdControlType::INCAPACITATE:
            return target->HasAuraType(SPELL_AURA_MOD_MECHANIC_IMMUNITY);
        case CrowdControlType::DISORIENT:
            return target->HasAuraType(SPELL_AURA_MOD_FEAR);
        case CrowdControlType::ROOT:
            return target->HasAuraType(SPELL_AURA_MOD_ROOT_IMMUNITY);
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
    if (target->IsElite())
        priority += 10.0f;

    // Distance penalty (prefer nearby targets)
    float distance = _bot->GetDistance(target);
    if (distance > 30.0f)
        priority *= 0.7f;

    return priority;
}

std::vector<uint32> CrowdControlManager::GetAvailableCCSpells() const
{
    std::vector<uint32> spells;

    if (!_bot)
        return spells;

    // TODO: Implement class-specific CC spell detection
    // This is a placeholder that should be replaced with actual spell checking
    // For example:
    // - Mage: Polymorph (118)
    // - Rogue: Sap (6770)
    // - Hunter: Freezing Trap (1499)
    // - Warlock: Fear (5782), Banish (710)
    // - Priest: Shackle Undead (9484), Mind Control (605)
    // - etc.

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
    if (_bot->GetPower(_bot->GetPowerType()) < spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask()))
        return false;

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
        if (spellInfo->GetMechanic() == MECHANIC_POLYMORPH ||
            spellInfo->HasEffect(SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_CONFUSE))
        {
            if (creatureType != CREATURE_TYPE_BEAST &&
                creatureType != CREATURE_TYPE_HUMANOID &&
                creatureType != CREATURE_TYPE_CRITTER)
            {
                return false;
            }
        }

        // Banish: works on demons and elementals
        if (spellInfo->GetMechanic() == MECHANIC_BANISH)
        {
            if (creatureType != CREATURE_TYPE_DEMON &&
                creatureType != CREATURE_TYPE_ELEMENTAL)
            {
                return false;
            }
        }

        // Shackle: works on undead
        if (spellInfo->GetMechanic() == MECHANIC_SHACKLE ||
            spellInfo->HasEffect(SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_SHAPESHIFT))
        {
            if (creatureType != CREATURE_TYPE_UNDEAD)
            {
                return false;
            }
        }

        // Fear: works on humanoids and beasts (generally)
        if (spellInfo->GetMechanic() == MECHANIC_FEAR)
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
    if (target->IsImmunedToSpell(spellInfo))
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
