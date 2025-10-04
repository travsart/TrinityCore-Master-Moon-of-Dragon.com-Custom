/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "TargetScanner.h"
#include "Player.h"
#include "Creature.h"
#include "Group.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "CreatureAI.h"
#include <algorithm>

namespace Playerbot
{
    TargetScanner::TargetScanner(Player* bot) :
        m_bot(bot),
        m_scanMode(ScanMode::AGGRESSIVE),
        m_lastScanTime(0),
        m_scanInterval(SCAN_INTERVAL_NORMAL),
        m_lastResultsTime(0),
        m_baseRange(20.0f),
        m_maxRange(40.0f),
        m_preferRanged(false),
        m_avoidElites(false)
    {
        // Configure based on class
        switch (m_bot->GetClass())
        {
            case CLASS_HUNTER:
                m_baseRange = 35.0f;
                m_maxRange = 41.0f;  // Max hunter range
                m_preferRanged = true;
                m_scanInterval = SCAN_INTERVAL_NORMAL;
                break;

            case CLASS_MAGE:
            case CLASS_WARLOCK:
                m_baseRange = 30.0f;
                m_maxRange = 36.0f;
                m_preferRanged = true;
                m_scanInterval = SCAN_INTERVAL_NORMAL;
                break;

            case CLASS_PRIEST:
                m_baseRange = 27.0f;
                m_maxRange = 36.0f;
                m_preferRanged = true;
                m_avoidElites = true;  // Priests should be more cautious
                m_scanInterval = SCAN_INTERVAL_NORMAL;
                break;

            case CLASS_SHAMAN:
            case CLASS_DRUID:
                m_baseRange = 25.0f;
                m_maxRange = 36.0f;
                m_preferRanged = false;  // Hybrid, depends on spec
                m_scanInterval = SCAN_INTERVAL_NORMAL;
                break;

            case CLASS_WARRIOR:
            case CLASS_PALADIN:
                m_baseRange = 15.0f;
                m_maxRange = 25.0f;
                m_preferRanged = false;
                m_scanInterval = SCAN_INTERVAL_COMBAT;
                break;

            case CLASS_ROGUE:
                m_baseRange = 10.0f;  // Rogues want to get close for stealth opener
                m_maxRange = 20.0f;
                m_preferRanged = false;
                m_scanInterval = SCAN_INTERVAL_COMBAT;
                break;

            case CLASS_DEATH_KNIGHT:
                m_baseRange = 20.0f;  // Death grip range
                m_maxRange = 30.0f;
                m_preferRanged = false;
                m_scanInterval = SCAN_INTERVAL_COMBAT;
                break;

            case CLASS_MONK:
                m_baseRange = 15.0f;
                m_maxRange = 25.0f;
                m_preferRanged = false;
                m_scanInterval = SCAN_INTERVAL_COMBAT;
                break;

            case CLASS_DEMON_HUNTER:
                m_baseRange = 20.0f;
                m_maxRange = 30.0f;
                m_preferRanged = false;
                m_scanInterval = SCAN_INTERVAL_COMBAT;
                break;

            default:
                m_baseRange = 20.0f;
                m_maxRange = 30.0f;
                break;
        }

        // Adjust for low level (more cautious)
        if (m_bot->GetLevel() < 20)
        {
            m_baseRange *= 0.75f;
            m_maxRange *= 0.75f;
            m_avoidElites = true;
        }
    }

    TargetScanner::~TargetScanner() = default;

    Unit* TargetScanner::FindNearestHostile(float range)
    {
        if (m_scanMode == ScanMode::PASSIVE)
            return nullptr;

        if (range == 0.0f)
            range = GetScanRadius();

        // Use cached results if recent enough
        uint32 now = getMSTime();
        if (m_lastResultsTime > 0 && (now - m_lastResultsTime) < SCAN_RESULTS_CACHE)
        {
            for (const auto& result : m_lastScanResults)
            {
                if (result.target && IsValidTarget(result.target) && result.distance <= range)
                    return result.target;
            }
        }

        // Perform new scan
        m_lastScanResults.clear();

        // Grid search for hostile units
        std::list<Unit*> targets;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck checker(m_bot, m_bot, range);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(m_bot, targets, checker);
        Cell::VisitAllObjects(m_bot, searcher, range);

        Unit* nearest = nullptr;
        float nearestDist = range + 1.0f;

        for (Unit* unit : targets)
        {
            if (!unit || !IsValidTarget(unit))
                continue;

            float dist = m_bot->GetDistance(unit);
            if (dist < nearestDist)
            {
                nearestDist = dist;
                nearest = unit;
            }
        }

        m_lastResultsTime = now;
        return nearest;
    }

    Unit* TargetScanner::FindBestTarget(float range)
    {
        if (m_scanMode == ScanMode::PASSIVE)
            return nullptr;

        if (range == 0.0f)
            range = GetScanRadius();

        std::vector<Unit*> hostiles = FindAllHostiles(range);
        if (hostiles.empty())
            return nullptr;

        // Build scan results with priorities
        std::vector<ScanResult> results;
        for (Unit* unit : hostiles)
        {
            if (!unit || !IsValidTarget(unit))
                continue;

            ScanResult result;
            result.target = unit;
            result.distance = m_bot->GetDistance(unit);
            result.threat = GetThreatValue(unit);
            result.priority = GetTargetPriority(unit);

            if (result.priority > PRIORITY_AVOID)
                results.push_back(result);
        }

        if (results.empty())
            return nullptr;

        // Sort by priority and distance
        std::sort(results.begin(), results.end());

        // Return best target that we should engage
        for (const auto& result : results)
        {
            if (ShouldEngage(result.target))
                return result.target;
        }

        return nullptr;
    }

    std::vector<Unit*> TargetScanner::FindAllHostiles(float range)
    {
        std::vector<Unit*> hostiles;

        if (m_scanMode == ScanMode::PASSIVE)
            return hostiles;

        if (range == 0.0f)
            range = GetScanRadius();

        // Grid search
        std::list<Unit*> targets;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck checker(m_bot, m_bot, range);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(m_bot, targets, checker);
        Cell::VisitAllObjects(m_bot, searcher, range);

        for (Unit* unit : targets)
        {
            if (unit && IsValidTarget(unit))
                hostiles.push_back(unit);
        }

        return hostiles;
    }

    bool TargetScanner::IsValidTarget(Unit* target) const
    {
        if (!target || !target->IsAlive())
            return false;

        // Check if unit is attackable
        if (!m_bot->IsValidAttackTarget(target))
            return false;

        // Check if blacklisted
        if (IsBlacklisted(target->GetGUID()))
            return false;

        // Don't attack friendly units
        if (m_bot->IsFriendlyTo(target))
            return false;

        // Don't attack units we can't see
        if (!m_bot->IsWithinLOSInMap(target))
            return false;

        // Don't attack critters unless they're hostile
        if (target->GetTypeId() == TYPEID_UNIT)
        {
            Creature* creature = target->ToCreature();
            if (creature->GetCreatureTemplate()->type == CREATURE_TYPE_CRITTER &&
                !creature->IsHostileTo(m_bot))
                return false;
        }

        // Don't attack units that are evading
        if (target->HasUnitState(UNIT_STATE_EVADE))
            return false;

        // Don't attack units that are immune
        if (target->HasUnitState(UNIT_STATE_UNATTACKABLE))
            return false;

        return true;
    }

    bool TargetScanner::ShouldEngage(Unit* target) const
    {
        if (!target || !IsValidTarget(target))
            return false;

        // Check scan mode
        if (m_scanMode == ScanMode::PASSIVE)
            return false;

        if (m_scanMode == ScanMode::DEFENSIVE)
        {
            // Only engage if target is attacking us or our group
            if (!IsAttackingGroup(target))
                return false;
        }

        // Don't engage if bot is low health
        float healthPct = m_bot->GetHealthPct();
        if (healthPct < 30.0f)
            return false;

        // Be more cautious at low health
        if (healthPct < 50.0f && target->GetLevel() > m_bot->GetLevel())
            return false;

        // Level difference check
        int32 levelDiff = int32(target->GetLevel()) - int32(m_bot->GetLevel());

        // Don't attack targets too high level
        if (levelDiff > 3)
            return false;

        // Grey level mobs (too low level)
        if (levelDiff < -7 && m_bot->GetLevel() > 10)
        {
            // Only engage if they're attacking us
            if (!IsAttackingGroup(target))
                return false;
        }

        // Elite checks
        if (target->GetTypeId() == TYPEID_UNIT)
        {
            Creature* creature = target->ToCreature();
            bool isElite = creature->IsElite() || creature->IsDungeonBoss();

            if (isElite)
            {
                // Don't solo elites if configured to avoid them
                if (m_avoidElites && !m_bot->GetGroup())
                    return false;

                // Don't solo elites more than 1 level higher
                if (levelDiff > 1 && !m_bot->GetGroup())
                    return false;

                // Don't engage elite if low on resources
                if (m_bot->GetPowerPct(m_bot->GetPowerType()) < 50.0f)
                    return false;
            }
        }

        // Don't engage if target is already fighting multiple players
        if (IsTargetInCombatWithOthers(target))
        {
            // Unless it's attacking our group
            if (!IsAttackingGroup(target))
                return false;
        }

        // Check if we can actually reach the target
        if (!CanReachTarget(target))
            return false;

        return true;
    }

    bool TargetScanner::CanReachTarget(Unit* target) const
    {
        if (!target)
            return false;

        float dist = m_bot->GetDistance(target);
        float maxRange = GetMaxEngageRange();

        // Too far away
        if (dist > maxRange)
            return false;

        // Check if path exists (basic check)
        if (!m_bot->IsWithinLOSInMap(target))
            return false;

        // For ranged classes, check if we're in range
        if (m_preferRanged)
        {
            // Check if we have a ranged attack spell in range
            // This is simplified - in production you'd check actual spell ranges
            if (dist <= 36.0f)
                return true;
        }

        // For melee, need to be able to get close
        if (dist <= 5.0f || m_bot->IsWithinMeleeRange(target))
            return true;

        // Can we get there?
        return dist <= 40.0f;  // Reasonable chase distance
    }

    uint8 TargetScanner::GetTargetPriority(Unit* target) const
    {
        if (!target)
            return PRIORITY_AVOID;

        // Highest priority: attacking us or group
        if (IsAttackingGroup(target))
            return PRIORITY_CRITICAL;

        // High priority: casters and healers
        if (IsCaster(target) || IsHealer(target))
            return PRIORITY_CASTER;

        // Check creature rank
        if (target->GetTypeId() == TYPEID_UNIT)
        {
            Creature* creature = target->ToCreature();

            // Elite mobs
            if (creature->IsElite() || creature->IsDungeonBoss())
                return PRIORITY_ELITE;

            // Check level difference
            int32 levelDiff = int32(target->GetLevel()) - int32(m_bot->GetLevel());

            // Grey mobs (trivial)
            if (levelDiff < -7)
                return PRIORITY_TRIVIAL;

            // Red mobs (dangerous)
            if (levelDiff > 3)
                return PRIORITY_AVOID;
        }

        return PRIORITY_NORMAL;
    }

    float TargetScanner::GetThreatValue(Unit* target) const
    {
        if (!target)
            return 0.0f;

        float threat = 0.0f;

        // Base threat from level difference
        int32 levelDiff = int32(target->GetLevel()) - int32(m_bot->GetLevel());
        threat = 100.0f + (levelDiff * 10.0f);

        // Increase threat for elites
        if (target->GetTypeId() == TYPEID_UNIT)
        {
            Creature* creature = target->ToCreature();
            if (creature->IsElite())
                threat *= 2.0f;
            if (creature->IsDungeonBoss())
                threat *= 5.0f;
        }

        // Increase threat for casters
        if (IsCaster(target))
            threat *= 1.5f;

        // Increase threat if target is attacking us
        if (target->GetVictim() == m_bot)
            threat *= 3.0f;

        // Increase threat if attacking group member
        if (IsAttackingGroup(target))
            threat *= 2.0f;

        // Reduce threat based on health
        threat *= target->GetHealthPct() / 100.0f;

        return threat;
    }

    float TargetScanner::GetScanRadius() const
    {
        // In combat, scan closer
        if (m_bot->IsInCombat())
            return m_baseRange * 0.75f;

        // Use base range
        return m_baseRange;
    }

    float TargetScanner::GetMaxEngageRange() const
    {
        return m_maxRange;
    }

    bool TargetScanner::ShouldScan(uint32 currentTime) const
    {
        if (m_lastScanTime == 0)
            return true;

        uint32 interval = m_scanInterval;

        // Scan more frequently in combat
        if (m_bot->IsInCombat())
            interval = SCAN_INTERVAL_COMBAT;
        // Scan less frequently when idle
        else if (!m_bot->isMoving())
            interval = SCAN_INTERVAL_IDLE;

        return (currentTime - m_lastScanTime) >= interval;
    }

    void TargetScanner::UpdateScanTime(uint32 currentTime)
    {
        m_lastScanTime = currentTime;
    }

    bool TargetScanner::IsAttackingGroup(Unit* target) const
    {
        if (!target || !target->GetVictim())
            return false;

        Unit* victim = target->GetVictim();

        // Attacking bot
        if (victim == m_bot)
            return true;

        // Attacking group member
        if (Group* group = m_bot->GetGroup())
        {
            if (group->IsMember(victim->GetGUID()))
                return true;
        }

        return false;
    }

    bool TargetScanner::IsCaster(Unit* target) const
    {
        if (!target)
            return false;

        // Check if currently casting
        if (target->HasUnitState(UNIT_STATE_CASTING))
            return true;

        // Check creature type
        if (target->GetTypeId() == TYPEID_UNIT)
        {
            Creature* creature = target->ToCreature();
            if (creature->GetCreatureTemplate()->unit_class == 2 || // Paladin
                creature->GetCreatureTemplate()->unit_class == 5 || // Priest
                creature->GetCreatureTemplate()->unit_class == 8)   // Mage
                return true;
        }

        return false;
    }

    bool TargetScanner::IsHealer(Unit* target) const
    {
        if (!target)
            return false;

        // Simple check - priests and paladins are often healers
        if (target->GetTypeId() == TYPEID_UNIT)
        {
            Creature* creature = target->ToCreature();
            if (creature->GetCreatureTemplate()->unit_class == 5 || // Priest
                creature->GetCreatureTemplate()->unit_class == 2)   // Paladin
            {
                // More sophisticated check would look at spell list
                return true;
            }
        }

        return false;
    }

    bool TargetScanner::IsDangerous(Unit* target) const
    {
        if (!target)
            return false;

        // Elite or higher
        if (target->GetTypeId() == TYPEID_UNIT)
        {
            Creature* creature = target->ToCreature();
            if (creature->IsElite() || creature->IsDungeonBoss())
                return true;
        }

        // Much higher level
        if (target->GetLevel() > m_bot->GetLevel() + 2)
            return true;

        // Multiple adds - simplified check without calling non-const method
        // Check for nearby enemies in combat (simplified version)
        // In full implementation, would cache scan results or make FindAllHostiles const

        return false;
    }

    void TargetScanner::AddToBlacklist(ObjectGuid guid, uint32 duration)
    {
        uint32 expireTime = getMSTime() + duration;

        // Check if already blacklisted
        for (auto& entry : m_blacklist)
        {
            if (entry.guid == guid)
            {
                entry.expireTime = expireTime;
                return;
            }
        }

        // Add new entry
        BlacklistEntry entry;
        entry.guid = guid;
        entry.expireTime = expireTime;
        m_blacklist.push_back(entry);
    }

    void TargetScanner::RemoveFromBlacklist(ObjectGuid guid)
    {
        m_blacklist.erase(
            std::remove_if(m_blacklist.begin(), m_blacklist.end(),
                [guid](const BlacklistEntry& entry) { return entry.guid == guid; }),
            m_blacklist.end()
        );
    }

    bool TargetScanner::IsBlacklisted(ObjectGuid guid) const
    {
        for (const auto& entry : m_blacklist)
        {
            if (entry.guid == guid)
                return true;
        }
        return false;
    }

    void TargetScanner::UpdateBlacklist(uint32 currentTime)
    {
        m_blacklist.erase(
            std::remove_if(m_blacklist.begin(), m_blacklist.end(),
                [currentTime](const BlacklistEntry& entry) { return entry.expireTime <= currentTime; }),
            m_blacklist.end()
        );
    }

    float TargetScanner::CalculateEngageDistance(Unit* target) const
    {
        if (!target)
            return 0.0f;

        float baseDistance = m_preferRanged ? 30.0f : 5.0f;

        // Adjust for target type
        if (target->GetTypeId() == TYPEID_UNIT)
        {
            Creature* creature = target->ToCreature();

            // Stay further from elites
            if (creature->IsElite())
                baseDistance += 5.0f;

            // Get closer to casters to interrupt
            if (IsCaster(target) && !m_preferRanged)
                baseDistance = 5.0f;
        }

        return baseDistance;
    }

    bool TargetScanner::CheckLineOfSight(Unit* target) const
    {
        return target && m_bot->IsWithinLOSInMap(target);
    }

    bool TargetScanner::IsTargetInCombatWithOthers(Unit* target) const
    {
        if (!target || !target->IsInCombat())
            return false;

        // Check if fighting other players (not in our group)
        Unit* victim = target->GetVictim();
        if (!victim)
            return false;

        if (victim->GetTypeId() == TYPEID_PLAYER)
        {
            // Not us
            if (victim != m_bot)
            {
                // Not in our group
                if (!m_bot->GetGroup() || !m_bot->GetGroup()->IsMember(victim->GetGUID()))
                    return true;
            }
        }

        return false;
    }

    float TargetScanner::GetClassBasedRange() const
    {
        // This could be expanded based on spec detection
        return m_baseRange;
    }
}  // namespace Playerbot