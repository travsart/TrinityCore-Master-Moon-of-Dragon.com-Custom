/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 */

#include "HealingTargetSelector.h"
#include "Player.h"
#include "Group.h"
#include "Unit.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "Log.h"
#include <algorithm>

namespace bot { namespace ai {

// TargetPriority implementation
float TargetPriority::CalculateScore() const
{
    if (!player || player->isDead())
        return 0.0f;

    // Base score: health deficit × role priority × distance
    float score = healthDeficit * rolePriority * distanceFactor;

    // Add debuff urgency (10 points per dispellable debuff)
    score += debuffCount * 10.0f;

    // Add threat factor bonus (high threat targets are more important to keep alive)
    score += threatFactor * 15.0f;

    // Reduce priority if already being healed (avoid overheal)
    if (hasIncomingHeals)
        score *= 0.7f;

    // Boost main tank priority
    if (isMainTank)
        score *= 1.2f;

    return score;
}

// HealingTargetSelector implementation
Player* HealingTargetSelector::SelectTarget(Player* healer, float range, float minHealthPercent)
{
    if (!healer)
        return nullptr;

    std::vector<TargetPriority> priorities = GetInjuredAllies(healer, range, minHealthPercent);

    if (priorities.empty())
        return nullptr;

    // Return highest priority target
    return priorities.front().player;
}

std::vector<TargetPriority> HealingTargetSelector::GetInjuredAllies(
    Player* healer,
    float range,
    float minHealthPercent)
{
    std::vector<TargetPriority> priorities;

    if (!healer)
        return priorities;

    Group* group = healer->GetGroup();
    if (!group)
    {
        // Solo: heal self if injured
        float healthPct = healer->GetHealthPct();
        if (healthPct < minHealthPercent)
        {
            TargetPriority priority;
            priority.player = healer;
            priority.healthDeficit = 100.0f - healthPct;
            priority.rolePriority = CalculateRolePriority(healer);
            priority.distanceFactor = 1.0f;
            priority.hasIncomingHeals = HasIncomingHeals(healer);
            priority.debuffCount = CountDispellableDebuffs(healer, DISPEL_ALL);
            priority.threatFactor = CalculateThreatFactor(healer);
            priority.isMainTank = false;

            priorities.push_back(priority);
        }
        return priorities;
    }

    // Group: evaluate all members
    std::vector<Player*> members = GetGroupMembersInRange(healer, range);

    for (Player* member : members)
    {
        if (!member || member->isDead())
            continue;

        float healthPct = member->GetHealthPct();
        if (healthPct >= minHealthPercent)
            continue;

        TargetPriority priority;
        priority.player = member;
        priority.healthDeficit = 100.0f - healthPct;
        priority.rolePriority = CalculateRolePriority(member);
        priority.distanceFactor = CalculateDistanceFactor(healer, member, range);
        priority.hasIncomingHeals = HasIncomingHeals(member);
        priority.debuffCount = CountDispellableDebuffs(member, DISPEL_ALL);
        priority.threatFactor = CalculateThreatFactor(member);
        priority.isMainTank = IsMainTank(member);

        priorities.push_back(priority);
    }

    // Sort by priority (highest first)
    std::sort(priorities.begin(), priorities.end(),
        [](const TargetPriority& a, const TargetPriority& b) {
            return a.CalculateScore() > b.CalculateScore();
        });

    return priorities;
}

bool HealingTargetSelector::NeedsDispel(Player* target, DispelType type)
{
    return CountDispellableDebuffs(target, type) > 0;
}

std::vector<Player*> HealingTargetSelector::GetTargetsNeedingDispel(
    Player* healer,
    DispelType type,
    float range)
{
    std::vector<Player*> targets;

    if (!healer)
        return targets;

    std::vector<Player*> members = GetGroupMembersInRange(healer, range);

    // Build list with priority
    struct DispelTarget
    {
        Player* player;
        uint32 debuffCount;
        float priority;
    };

    std::vector<DispelTarget> dispelTargets;

    for (Player* member : members)
    {
        if (!member || member->isDead())
            continue;

        uint32 debuffs = CountDispellableDebuffs(member, type);
        if (debuffs == 0)
            continue;

        DispelTarget dt;
        dt.player = member;
        dt.debuffCount = debuffs;

        // Priority: role × (1.0 - health) × debuffCount
        float rolePriority = CalculateRolePriority(member);
        float healthFactor = 1.0f - (member->GetHealthPct() / 100.0f);
        dt.priority = rolePriority * healthFactor * debuffs;

        dispelTargets.push_back(dt);
    }

    // Sort by priority
    std::sort(dispelTargets.begin(), dispelTargets.end(),
        [](const DispelTarget& a, const DispelTarget& b) {
            return a.priority > b.priority;
        });

    // Extract sorted players
    for (const auto& dt : dispelTargets)
        targets.push_back(dt.player);

    return targets;
}

Unit* HealingTargetSelector::SelectAoEHealingTarget(
    Player* healer,
    uint32 minTargets,
    float range)
{
    if (!healer)
        return nullptr;

    std::vector<Player*> members = GetGroupMembersInRange(healer, range * 1.5f);

    Unit* bestTarget = nullptr;
    float bestScore = 0.0f;

    // Evaluate each member as potential AoE center
    for (Player* center : members)
    {
        if (!center)
            continue;

        float score = CalculateAoEHealingScore(center, healer, range);

        if (score > bestScore)
        {
            bestScore = score;
            bestTarget = center;
        }
    }

    // Only return if we found a cluster with enough targets
    if (bestTarget)
    {
        uint32 targetCount = 0;
        for (Player* member : members)
        {
            if (!member || member->isDead())
                continue;

            if (member->GetDistance(bestTarget) <= range && member->GetHealthPct() < 95.0f)
                ++targetCount;
        }

        if (targetCount < minTargets)
            return nullptr;
    }

    return bestTarget;
}

bool HealingTargetSelector::IsHealingNeeded(Player* healer, float urgencyThreshold)
{
    if (!healer)
        return false;

    Group* group = healer->GetGroup();
    if (!group)
    {
        // Solo: check self
        return healer->GetHealthPct() < urgencyThreshold;
    }

    // Group: check any member
    std::vector<Player*> members = GetGroupMembersInRange(healer, 40.0f);

    for (Player* member : members)
    {
        if (member && !member->isDead() && member->GetHealthPct() < urgencyThreshold)
            return true;
    }

    return false;
}

float HealingTargetSelector::GetIncomingHealAmount(Player* target)
{
    if (!target)
        return 0.0f;

    float incomingHeals = 0.0f;

    // TODO: Implement sophisticated incoming heal tracking
    // For now, this is a placeholder that can be enhanced
    // Ideas:
    // - Track active HoTs (Renew, Rejuvenation, Riptide)
    // - Track pending direct heals (cast in progress by other healers)
    // - Track shields (Power Word: Shield, Earth Shield)

    return incomingHeals;
}

float HealingTargetSelector::PredictHealthInSeconds(Player* target, float seconds)
{
    if (!target)
        return 0.0f;

    float currentHealth = target->GetHealthPct();
    float incomingHeals = GetIncomingHealAmount(target);

    // TODO: Implement damage prediction
    // For now, return current + incoming heals
    // Future: Track incoming damage (dots, predictable boss abilities)

    float incomingHealPercent = (incomingHeals / target->GetMaxHealth()) * 100.0f;
    float predictedHealth = currentHealth + incomingHealPercent;

    return std::min(predictedHealth, 100.0f);
}

// Private helper functions

float HealingTargetSelector::CalculateRolePriority(Player* target)
{
    if (!target)
        return 1.0f;

    // Detect role based on spec/class
    // TODO: Integrate with bot role detection system if available

    // Simple heuristic for now
    switch (target->getClass())
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
        case CLASS_DRUID:  // Could be tank
        case CLASS_MONK:   // Could be tank
            // Check if tank spec (TODO: proper spec detection)
            return 2.0f;  // Tank priority

        case CLASS_PRIEST:
        case CLASS_SHAMAN:
            return 1.5f;  // Healer priority

        default:
            return 1.0f;  // DPS priority
    }
}

float HealingTargetSelector::CalculateDistanceFactor(Player* healer, Player* target, float maxRange)
{
    if (!healer || !target)
        return 0.0f;

    float distance = healer->GetDistance(target);

    if (distance > maxRange)
        return 0.0f;

    // Linear falloff: 1.0 at 0 yards, 0.0 at maxRange
    return 1.0f - (distance / maxRange);
}

bool HealingTargetSelector::HasIncomingHeals(Player* target)
{
    if (!target)
        return false;

    // Check if target is being healed by other group members
    Group* group = target->GetGroup();
    if (!group)
        return false;

    // Iterate through group members to find healing spells being cast on target
    for (GroupReference* itr : *group)
    {
        Player* member = itr->GetSource();
        if (!member || member == target)
            continue;

        // Check if member is currently casting
        if (Spell* spell = member->GetCurrentSpell(CURRENT_GENERIC_SPELL))
        {
            // Check if spell targets our target
            if (spell->m_targets.GetUnitTarget() == target)
            {
                // Check if spell is a healing spell
                SpellInfo const* spellInfo = spell->GetSpellInfo();
                if (spellInfo && spellInfo->IsPositive() && spellInfo->HasEffect(SPELL_EFFECT_HEAL))
                {
                    TC_LOG_TRACE("playerbot", "HealingTargetSelector: Target {} has incoming heal from {}",
                                target->GetName(), member->GetName());
                    return true;
                }
            }
        }

        // Also check channeled spells
        if (Spell* channelSpell = member->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        {
            if (channelSpell->m_targets.GetUnitTarget() == target)
            {
                SpellInfo const* spellInfo = channelSpell->GetSpellInfo();
                if (spellInfo && spellInfo->IsPositive() && spellInfo->HasEffect(SPELL_EFFECT_HEAL))
                {
                    TC_LOG_TRACE("playerbot", "HealingTargetSelector: Target {} has incoming channeled heal from {}",
                                target->GetName(), member->GetName());
                    return true;
                }
            }
        }
    }

    return false;
}

uint32 HealingTargetSelector::CountDispellableDebuffs(Player* target, DispelType type)
{
    if (!target)
        return 0;

    uint32 count = 0;

    // Iterate through auras
    Unit::AuraApplicationMap const& auras = target->GetAppliedAuras();
    for (auto const& [spellId, aurApp] : auras)
    {
        if (!aurApp)
            continue;

        Aura* aura = aurApp->GetBase();
        if (!aura)
            continue;

        SpellInfo const* spellInfo = aura->GetSpellInfo();
        if (!spellInfo)
            continue;

        // Check if negative aura (debuff)
        if (!spellInfo->IsPositive())
        {
            // Check dispel type
            if (type == DISPEL_ALL || spellInfo->Dispel == type)
                ++count;
        }
    }

    return count;
}

float HealingTargetSelector::CalculateThreatFactor(Player* target)
{
    if (!target)
        return 0.0f;

    // TODO: Implement threat tracking
    // High threat targets are more important to keep alive
    // Integrate with ThreatManager if available

    return 0.0f;  // Placeholder
}

bool HealingTargetSelector::IsMainTank(Player* target)
{
    if (!target)
        return false;

    // TODO: Implement main tank detection
    // Check if player is designated as main tank in group/raid
    // Could check for tank role + highest threat

    return false;  // Placeholder
}

std::vector<Player*> HealingTargetSelector::GetGroupMembersInRange(Player* healer, float range)
{
    std::vector<Player*> members;

    if (!healer)
        return members;

    Group* group = healer->GetGroup();
    if (!group)
    {
        // Solo: only self
        members.push_back(healer);
        return members;
    }

    // Group: iterate members
    for (GroupReference& ref : group->GetMembers())
    {
        Player* member = ref.GetSource();
        if (!member || member->isDead())
            continue;

        // Check range
        if (healer->GetDistance(member) <= range)
            members.push_back(member);
    }

    return members;
}

float HealingTargetSelector::CalculateAoEHealingScore(Unit* position, Player* healer, float range)
{
    if (!position || !healer)
        return 0.0f;

    std::vector<Player*> members = GetGroupMembersInRange(healer, range * 1.5f);

    uint32 injuredCount = 0;
    float totalDeficit = 0.0f;

    for (Player* member : members)
    {
        if (!member || member->isDead())
            continue;

        float distance = member->GetDistance(position);
        if (distance > range)
            continue;

        float healthPct = member->GetHealthPct();
        if (healthPct >= 95.0f)
            continue;

        ++injuredCount;
        totalDeficit += (100.0f - healthPct);
    }

    // Score = number of injured × average deficit
    if (injuredCount == 0)
        return 0.0f;

    float avgDeficit = totalDeficit / injuredCount;
    return injuredCount * avgDeficit;
}

}} // namespace bot::ai
