/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "RestorationSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"
#include "Group.h"

namespace Playerbot
{

RestorationSpecialization::RestorationSpecialization(Player* bot)
    : DruidSpecialization(bot)
    , _treeOfLifeRemaining(0)
    , _inTreeForm(false)
    , _lastTreeFormShift(0)
    , _naturesSwiftnessReady(0)
    , _lastNaturesSwiftness(0)
    , _tranquilityReady(0)
    , _lastTranquility(0)
    , _lastHealCheck(0)
    , _lastHotCheck(0)
    , _lastGroupScan(0)
    , _emergencyMode(false)
    , _emergencyStartTime(0)
    , _totalHealingDone(0)
    , _overhealingDone(0)
    , _manaSpent(0)
{
    _currentForm = DruidForm::HUMANOID;
}

void RestorationSpecialization::UpdateRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    UpdateHealing();
    UpdateHealOverTimeManagement();
    UpdateFormManagement();
    UpdateNaturesSwiftness();
    UpdateTranquility();
    ManageMana();

    // Emergency healing takes priority
    if (IsEmergencyHealing())
    {
        HandleEmergencyHealing();
        return;
    }

    // Group healing assessment
    if (ShouldUseGroupHeals())
    {
        UpdateGroupHealing();
        return;
    }

    // Regular healing rotation
    ::Unit* healTarget = GetBestHealTarget();
    if (healTarget)
    {
        HealTarget(healTarget);
    }
}

void RestorationSpecialization::UpdateBuffs()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Maintain Mark of the Wild
    if (!bot->HasAura(MARK_OF_THE_WILD) && bot->HasSpell(MARK_OF_THE_WILD))
    {
        bot->CastSpell(bot, MARK_OF_THE_WILD, false);
    }

    // Maintain Thorns
    if (!bot->HasAura(THORNS) && bot->HasSpell(THORNS))
    {
        bot->CastSpell(bot, THORNS, false);
    }

    UpdateFormManagement();
}

void RestorationSpecialization::UpdateCooldowns(uint32 diff)
{
    for (auto& cooldown : _cooldowns)
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;

    if (_treeOfLifeRemaining > diff)
        _treeOfLifeRemaining -= diff;
    else
    {
        _treeOfLifeRemaining = 0;
        _inTreeForm = false;
    }

    if (_lastTreeFormShift > diff)
        _lastTreeFormShift -= diff;
    else
        _lastTreeFormShift = 0;

    if (_naturesSwiftnessReady > diff)
        _naturesSwiftnessReady -= diff;
    else
        _naturesSwiftnessReady = 0;

    if (_lastNaturesSwiftness > diff)
        _lastNaturesSwiftness -= diff;
    else
        _lastNaturesSwiftness = 0;

    if (_tranquilityReady > diff)
        _tranquilityReady -= diff;
    else
        _tranquilityReady = 0;

    if (_lastTranquility > diff)
        _lastTranquility -= diff;
    else
        _lastTranquility = 0;

    if (_lastFormShift > diff)
        _lastFormShift -= diff;
    else
        _lastFormShift = 0;

    if (_lastHealCheck > diff)
        _lastHealCheck -= diff;
    else
        _lastHealCheck = 0;

    if (_lastHotCheck > diff)
        _lastHotCheck -= diff;
    else
        _lastHotCheck = 0;

    if (_lastGroupScan > diff)
        _lastGroupScan -= diff;
    else
        _lastGroupScan = 0;

    if (_emergencyStartTime > diff)
        _emergencyStartTime -= diff;
    else
        _emergencyStartTime = 0;
}

bool RestorationSpecialization::CanUseAbility(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    if (!CanCastInCurrentForm(spellId))
        return false;

    return HasEnoughResource(spellId);
}

void RestorationSpecialization::OnCombatStart(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Prepare for healing during combat
    _emergencyMode = false;
    _emergencyStartTime = 0;

    // Enter Tree of Life form if available and beneficial
    if (ShouldUseTreeForm())
        EnterTreeOfLifeForm();
}

void RestorationSpecialization::OnCombatEnd()
{
    _emergencyMode = false;
    _emergencyStartTime = 0;
    _treeOfLifeRemaining = 0;
    _inTreeForm = false;
    _cooldowns.clear();
    _regrowthTimers.clear();
    _lifebloomStacks.clear();
    _activeHoTs.clear();
    _healQueue = std::priority_queue<DruidHealTarget>();
}

bool RestorationSpecialization::HasEnoughResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return true;

    // Special cases for instant abilities
    switch (spellId)
    {
        case NATURES_SWIFTNESS:
            return _naturesSwiftnessReady == 0;
        case TRANQUILITY:
            return _tranquilityReady == 0;
        default:
            break;
    }

    auto powerCosts = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask()); uint32 manaCost = 0; for (auto const& cost : powerCosts) { if (cost.Power == POWER_MANA) { manaCost = cost.Amount; break; } }
    return bot->GetPower(POWER_MANA) >= manaCost;
}

void RestorationSpecialization::ConsumeResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    switch (spellId)
    {
        case NATURES_SWIFTNESS:
            _naturesSwiftnessReady = NATURES_SWIFTNESS_COOLDOWN;
            _lastNaturesSwiftness = getMSTime();
            break;
        case TRANQUILITY:
            _tranquilityReady = TRANQUILITY_COOLDOWN;
            _lastTranquility = getMSTime();
            break;
        default:
            auto powerCosts = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask()); uint32 manaCost = 0; for (auto const& cost : powerCosts) { if (cost.Power == POWER_MANA) { manaCost = cost.Amount; break; } }
            if (bot->GetPower(POWER_MANA) >= manaCost)
            {
                bot->SetPower(POWER_MANA, bot->GetPower(POWER_MANA) - manaCost);
                _manaSpent += manaCost;
            }
            break;
    }
}

Position RestorationSpecialization::GetOptimalPosition(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return Position();

    // Stay at healing range but away from combat
    float distance = OPTIMAL_HEALING_RANGE * 0.7f;
    float angle = 0.0f;

    if (target)
    {
        angle = target->GetAngle(bot) + M_PI;
        return Position(
            target->GetPositionX() + distance * cos(angle),
            target->GetPositionY() + distance * sin(angle),
            target->GetPositionZ(),
            angle
        );
    }

    return Position();
}

float RestorationSpecialization::GetOptimalRange(::Unit* target)
{
    return OPTIMAL_HEALING_RANGE;
}

void RestorationSpecialization::UpdateFormManagement()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    DruidForm optimalForm = GetOptimalFormForSituation();
    if (_currentForm != optimalForm && ShouldShiftToForm(optimalForm))
    {
        ShiftToForm(optimalForm);
    }

    ManageTreeForm();
}

DruidForm RestorationSpecialization::GetOptimalFormForSituation()
{
    Player* bot = GetBot();
    if (!bot)
        return DruidForm::HUMANOID;

    // Tree of Life form when available and beneficial
    if (_inTreeForm)
        return DruidForm::TREE_OF_LIFE;

    return DruidForm::HUMANOID;
}

bool RestorationSpecialization::ShouldShiftToForm(DruidForm form)
{
    return _currentForm != form && _lastFormShift == 0;
}

void RestorationSpecialization::ShiftToForm(DruidForm form)
{
    CastShapeshift(form);
    _previousForm = _currentForm;
    _currentForm = form;
    _lastFormShift = 1500; // GCD
}

void RestorationSpecialization::UpdateDotHotManagement()
{
    uint32 now = getMSTime();

    // Only check HoTs periodically for performance
    if (now - _lastHotCheck < 1000)
        return;

    _lastHotCheck = now;

    // Clean up expired HoT timers
    for (auto it = _regrowthTimers.begin(); it != _regrowthTimers.end();)
    {
        if (now - it->second > REGROWTH_DURATION)
            it = _regrowthTimers.erase(it);
        else
            ++it;
    }

    // Update Lifebloom stacks
    for (auto it = _lifebloomStacks.begin(); it != _lifebloomStacks.end();)
    {
        if (now - it->second > LIFEBLOOM_DURATION)
            it = _lifebloomStacks.erase(it);
        else
            ++it;
    }

    RefreshExpiringHoTs();
}

bool RestorationSpecialization::ShouldApplyDoT(::Unit* target, uint32 spellId)
{
    // Restoration doesn't typically use DoTs in healing rotation
    return false;
}

bool RestorationSpecialization::ShouldApplyHoT(::Unit* target, uint32 spellId)
{
    if (!target)
        return false;

    switch (spellId)
    {
        case REJUVENATION:
            return !target->HasAura(REJUVENATION) && HasEnoughResource(REJUVENATION);
        case REGROWTH:
            return !target->HasAura(REGROWTH) && HasEnoughResource(REGROWTH);
        case LIFEBLOOM:
            return GetLifebloomStacks(target) < LIFEBLOOM_MAX_STACKS && HasEnoughResource(LIFEBLOOM);
        default:
            return false;
    }
}

void RestorationSpecialization::UpdateHealing()
{
    uint32 now = getMSTime();
    if (now - _lastHealCheck < 500) // Check every 500ms
        return;

    _lastHealCheck = now;
    PrioritizeHealing();
    PerformTriage();
}

void RestorationSpecialization::UpdateHealOverTimeManagement()
{
    // Scan group members for HoT application
    if (Group* group = GetBot()->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member || !member->IsInWorld())
                continue;

            if (member->GetDistance(GetBot()) > OPTIMAL_HEALING_RANGE)
                continue;

            // Apply Rejuvenation to injured members without it
            if (member->GetHealthPct() < 85.0f && ShouldApplyHoT(member, REJUVENATION))
            {
                ApplyHealingOverTime(member, REJUVENATION);
                return;
            }

            // Maintain Lifebloom on tank
            if (member->GetHealthPct() < 90.0f && ShouldApplyHoT(member, LIFEBLOOM))
            {
                ApplyHealingOverTime(member, LIFEBLOOM);
                return;
            }
        }
    }
}

void RestorationSpecialization::UpdateNaturesSwiftness()
{
    // Nature's Swiftness management for emergency instant heals
    if (ShouldUseNaturesSwiftness())
    {
        UseNaturesSwiftness();
    }
}

void RestorationSpecialization::UpdateTranquility()
{
    // Check if Tranquility should be used for group emergency
    if (ShouldCastTranquility())
    {
        CastTranquility();
    }
}

bool RestorationSpecialization::ShouldCastHealingTouch(::Unit* target)
{
    return target && target->GetHealthPct() < HEALING_TOUCH_THRESHOLD &&
           HasEnoughResource(HEALING_TOUCH) &&
           GetBot()->GetDistance(target) <= OPTIMAL_HEALING_RANGE;
}

bool RestorationSpecialization::ShouldCastRegrowth(::Unit* target)
{
    return target && target->GetHealthPct() < REGROWTH_THRESHOLD &&
           HasEnoughResource(REGROWTH) &&
           GetBot()->GetDistance(target) <= OPTIMAL_HEALING_RANGE;
}

bool RestorationSpecialization::ShouldCastRejuvenation(::Unit* target)
{
    return target && !target->HasAura(REJUVENATION) &&
           target->GetHealthPct() < 90.0f &&
           HasEnoughResource(REJUVENATION) &&
           GetBot()->GetDistance(target) <= OPTIMAL_HEALING_RANGE;
}

bool RestorationSpecialization::ShouldCastLifebloom(::Unit* target)
{
    return target && GetLifebloomStacks(target) < LIFEBLOOM_MAX_STACKS &&
           target->GetHealthPct() < 95.0f &&
           HasEnoughResource(LIFEBLOOM) &&
           GetBot()->GetDistance(target) <= OPTIMAL_HEALING_RANGE;
}

bool RestorationSpecialization::ShouldCastSwiftmend(::Unit* target)
{
    return target && (target->HasAura(REJUVENATION) || target->HasAura(REGROWTH)) &&
           target->GetHealthPct() < 40.0f &&
           HasEnoughResource(SWIFTMEND) &&
           GetBot()->GetDistance(target) <= OPTIMAL_HEALING_RANGE;
}

bool RestorationSpecialization::ShouldCastTranquility()
{
    if (!HasEnoughResource(TRANQUILITY))
        return false;

    // Use when multiple group members are low on health
    uint32 lowHealthCount = 0;
    if (Group* group = GetBot()->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (member && member->IsInWorld() && member->GetHealthPct() < 50.0f)
                lowHealthCount++;
        }
    }

    return lowHealthCount >= 3;
}

bool RestorationSpecialization::ShouldUseNaturesSwiftness()
{
    if (!IsNaturesSwiftnessReady())
        return false;

    // Use for emergency instant heals
    ::Unit* criticalTarget = GetBestHealTarget();
    return criticalTarget && criticalTarget->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD;
}

::Unit* RestorationSpecialization::GetBestHealTarget()
{
    ::Unit* bestTarget = nullptr;
    float lowestHealthPct = 100.0f;

    // Check self first
    Player* bot = GetBot();
    if (bot->GetHealthPct() < lowestHealthPct)
    {
        bestTarget = bot;
        lowestHealthPct = bot->GetHealthPct();
    }

    // Check group members
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member || !member->IsInWorld())
                continue;

            if (member->GetDistance(bot) > OPTIMAL_HEALING_RANGE)
                continue;

            if (member->GetHealthPct() < lowestHealthPct)
            {
                bestTarget = member;
                lowestHealthPct = member->GetHealthPct();
            }
        }
    }

    return bestTarget;
}

void RestorationSpecialization::HealTarget(::Unit* target)
{
    if (!target)
        return;

    float healthPct = target->GetHealthPct();

    // Emergency healing
    if (healthPct < EMERGENCY_HEALTH_THRESHOLD)
    {
        UseEmergencyHeals(target);
        return;
    }

    // Swiftmend for quick healing
    if (healthPct < 40.0f && ShouldCastSwiftmend(target))
    {
        CastSwiftmend(target);
        return;
    }

    // Regrowth for moderate damage
    if (healthPct < REGROWTH_THRESHOLD && ShouldCastRegrowth(target))
    {
        CastRegrowth(target);
        return;
    }

    // Healing Touch for heavy damage
    if (healthPct < HEALING_TOUCH_THRESHOLD && ShouldCastHealingTouch(target))
    {
        CastHealingTouch(target);
        return;
    }

    // Rejuvenation for light damage
    if (ShouldCastRejuvenation(target))
    {
        CastRejuvenation(target);
        return;
    }

    // Lifebloom maintenance
    if (ShouldCastLifebloom(target))
    {
        CastLifebloom(target);
        return;
    }
}

void RestorationSpecialization::PrioritizeHealing()
{
    // Clear and rebuild heal queue
    _healQueue = std::priority_queue<DruidHealTarget>();

    Player* bot = GetBot();

    // Add self to queue
    if (bot->GetHealthPct() < 95.0f)
    {
        DruidHealPriority priority = GetHealPriority(bot->GetHealthPct());
        _healQueue.emplace(bot, priority, bot->GetHealthPct(),
                          bot->GetMaxHealth() - bot->GetHealth());
    }

    // Add group members to queue
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member || !member->IsInWorld())
                continue;

            if (member->GetDistance(bot) > OPTIMAL_HEALING_RANGE)
                continue;

            if (member->GetHealthPct() < 95.0f)
            {
                DruidHealPriority priority = GetHealPriority(member->GetHealthPct());
                _healQueue.emplace(member, priority, member->GetHealthPct(),
                                  member->GetMaxHealth() - member->GetHealth());
            }
        }
    }
}

uint32 RestorationSpecialization::GetOptimalHealSpell(const DruidHealTarget& healTarget)
{
    float healthPct = healTarget.healthPercent;

    if (healthPct < EMERGENCY_HEALTH_THRESHOLD)
    {
        if (IsNaturesSwiftnessReady())
            return HEALING_TOUCH; // Instant with Nature's Swiftness
        return REGROWTH; // Fast cast
    }
    else if (healthPct < REGROWTH_THRESHOLD)
    {
        return REGROWTH;
    }
    else if (healthPct < HEALING_TOUCH_THRESHOLD)
    {
        return HEALING_TOUCH;
    }
    else
    {
        return REJUVENATION; // HoT for light damage
    }
}

void RestorationSpecialization::PerformTriage()
{
    if (_healQueue.empty())
        return;

    DruidHealTarget healTarget = _healQueue.top();
    _healQueue.pop();

    HealTarget(healTarget.target);
}

void RestorationSpecialization::ApplyHealingOverTime(::Unit* target, uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    switch (spellId)
    {
        case REJUVENATION:
            CastRejuvenation(target);
            break;
        case REGROWTH:
            if (ShouldCastRegrowth(target))
                CastRegrowth(target);
            break;
        case LIFEBLOOM:
            CastLifebloom(target);
            break;
        default:
            break;
    }
}

void RestorationSpecialization::RefreshExpiringHoTs()
{
    uint32 now = getMSTime();

    // Check for HoTs that need refreshing
    if (Group* group = GetBot()->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member || !member->IsInWorld())
                continue;

            if (member->GetDistance(GetBot()) > OPTIMAL_HEALING_RANGE)
                continue;

            // Refresh Rejuvenation if expiring soon
            if (member->HasAura(REJUVENATION))
            {
                if (Aura* aura = member->GetAura(REJUVENATION))
                {
                    if (aura->GetDuration() < 3000) // Less than 3 seconds remaining
                    {
                        CastRejuvenation(member);
                        return;
                    }
                }
            }

            // Refresh Lifebloom before it expires
            if (GetLifebloomStacks(member) > 0)
            {
                uint32 stackTime = GetLifebloomStackTime(member);
                if (now - stackTime > (LIFEBLOOM_DURATION - 2000)) // Refresh 2 seconds before expiry
                {
                    CastLifebloom(member);
                    return;
                }
            }
        }
    }
}

uint32 RestorationSpecialization::GetHoTRemainingTime(::Unit* target, uint32 spellId)
{
    if (!target)
        return 0;

    if (Aura* aura = target->GetAura(spellId))
        return aura->GetDuration();

    return 0;
}

void RestorationSpecialization::ManageLifebloomStack(::Unit* target)
{
    if (!target)
        return;

    uint32 currentStacks = GetLifebloomStacks(target);
    if (currentStacks < LIFEBLOOM_MAX_STACKS && ShouldCastLifebloom(target))
    {
        CastLifebloom(target);
    }
}

void RestorationSpecialization::UpdateGroupHealing()
{
    // Check if group healing spells are needed
    if (ShouldCastTranquility())
    {
        CastTranquility();
        return;
    }

    // Apply HoTs to multiple injured group members
    if (Group* group = GetBot()->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member || !member->IsInWorld())
                continue;

            if (member->GetDistance(GetBot()) > OPTIMAL_HEALING_RANGE)
                continue;

            if (member->GetHealthPct() < 80.0f && ShouldCastRejuvenation(member))
            {
                CastRejuvenation(member);
                return;
            }
        }
    }
}

bool RestorationSpecialization::ShouldUseGroupHeals()
{
    uint32 injuredCount = 0;

    if (Group* group = GetBot()->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (member && member->IsInWorld() && member->GetHealthPct() < 70.0f)
                injuredCount++;
        }
    }

    return injuredCount >= 2;
}

void RestorationSpecialization::HandleEmergencyHealing()
{
    ::Unit* criticalTarget = GetBestHealTarget();
    if (criticalTarget && criticalTarget->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD)
    {
        UseEmergencyHeals(criticalTarget);
    }
}

void RestorationSpecialization::UseEmergencyHeals(::Unit* target)
{
    if (!target)
        return;

    // Nature's Swiftness + Healing Touch for instant big heal
    if (IsNaturesSwiftnessReady())
    {
        UseNaturesSwiftness();
        CastInstantHealingTouch(target);
        return;
    }

    // Swiftmend if HoTs are present
    if (ShouldCastSwiftmend(target))
    {
        CastSwiftmend(target);
        return;
    }

    // Regrowth for quick heal
    if (ShouldCastRegrowth(target))
    {
        CastRegrowth(target);
        return;
    }
}

bool RestorationSpecialization::IsEmergencyHealing()
{
    ::Unit* criticalTarget = GetBestHealTarget();
    return criticalTarget && criticalTarget->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD;
}

void RestorationSpecialization::CastHealingTouch(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(HEALING_TOUCH))
    {
        bot->CastSpell(target, HEALING_TOUCH, false);
        ConsumeResource(HEALING_TOUCH);
        _totalHealingDone += 2000; // Approximate healing amount
    }
}

void RestorationSpecialization::CastRegrowth(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(REGROWTH))
    {
        bot->CastSpell(target, REGROWTH, false);
        ConsumeResource(REGROWTH);
        ApplyHealingOverTime(target, REGROWTH);
        _regrowthTimers[target->GetGUID()] = getMSTime();
        _totalHealingDone += 1500; // Approximate healing amount
    }
}

void RestorationSpecialization::CastRejuvenation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(REJUVENATION))
    {
        bot->CastSpell(target, REJUVENATION, false);
        ConsumeResource(REJUVENATION);
        ApplyHealingOverTime(target, REJUVENATION);
        _totalHealingDone += 1000; // Approximate healing amount
    }
}

void RestorationSpecialization::CastLifebloom(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(LIFEBLOOM))
    {
        bot->CastSpell(target, LIFEBLOOM, false);
        ConsumeResource(LIFEBLOOM);

        uint32 currentStacks = GetLifebloomStacks(target);
        _lifebloomStacks[target->GetGUID()] = std::min(currentStacks + 1, LIFEBLOOM_MAX_STACKS);
        _totalHealingDone += 800; // Approximate healing amount
    }
}

void RestorationSpecialization::CastSwiftmend(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(SWIFTMEND))
    {
        bot->CastSpell(target, SWIFTMEND, false);
        ConsumeResource(SWIFTMEND);
        _totalHealingDone += 2500; // Approximate healing amount
    }
}

void RestorationSpecialization::CastTranquility()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(TRANQUILITY))
    {
        bot->CastSpell(bot, TRANQUILITY, false);
        ConsumeResource(TRANQUILITY);
        _totalHealingDone += 5000; // Approximate group healing amount
    }
}

void RestorationSpecialization::CastInnervate(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(INNERVATE))
    {
        bot->CastSpell(target, INNERVATE, false);
        ConsumeResource(INNERVATE);
    }
}

void RestorationSpecialization::EnterTreeOfLifeForm()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->HasSpell(TREE_OF_LIFE_FORM) && !_inTreeForm && _lastTreeFormShift == 0)
    {
        ShiftToForm(DruidForm::TREE_OF_LIFE);
        _inTreeForm = true;
        _treeOfLifeRemaining = TREE_OF_LIFE_DURATION;
        _lastTreeFormShift = getMSTime();
    }
}

bool RestorationSpecialization::ShouldUseTreeForm()
{
    Player* bot = GetBot();
    if (!bot || !bot->HasSpell(TREE_OF_LIFE_FORM))
        return false;

    // Use Tree form when multiple group members need healing
    return ShouldUseGroupHeals() && !_inTreeForm;
}

void RestorationSpecialization::ManageTreeForm()
{
    if (_inTreeForm && _treeOfLifeRemaining == 0)
    {
        _inTreeForm = false;
        ShiftToForm(DruidForm::HUMANOID);
    }
}

void RestorationSpecialization::UseNaturesSwiftness()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(NATURES_SWIFTNESS))
    {
        bot->CastSpell(bot, NATURES_SWIFTNESS, false);
        ConsumeResource(NATURES_SWIFTNESS);
    }
}

bool RestorationSpecialization::IsNaturesSwiftnessReady()
{
    return _naturesSwiftnessReady == 0;
}

void RestorationSpecialization::CastInstantHealingTouch(::Unit* target)
{
    // Nature's Swiftness makes next spell instant
    CastHealingTouch(target);
}

void RestorationSpecialization::ManageMana()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    float manaPct = static_cast<float>(bot->GetPower(POWER_MANA)) /
                   static_cast<float>(bot->GetMaxPower(POWER_MANA));

    if (manaPct < MANA_CONSERVATION_THRESHOLD)
    {
        CastInnervateOptimal();
    }
}

void RestorationSpecialization::CastInnervateOptimal()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Cast Innervate on self when low on mana
    if (bot->GetPowerPct(POWER_MANA) < 30.0f && HasEnoughResource(INNERVATE))
    {
        CastInnervate(bot);
    }
}

bool RestorationSpecialization::ShouldConserveMana()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    return bot->GetPowerPct(POWER_MANA) < MANA_CONSERVATION_THRESHOLD * 100;
}

DruidHealPriority RestorationSpecialization::GetHealPriority(float healthPct)
{
    if (healthPct < 20.0f)
        return DruidHealPriority::EMERGENCY;
    else if (healthPct < 40.0f)
        return DruidHealPriority::CRITICAL;
    else if (healthPct < 70.0f)
        return DruidHealPriority::MODERATE;
    else if (healthPct < 90.0f)
        return DruidHealPriority::MAINTENANCE;
    else
        return DruidHealPriority::FULL;
}

uint32 RestorationSpecialization::GetLifebloomStacks(::Unit* target)
{
    if (!target)
        return 0;

    auto it = _lifebloomStacks.find(target->GetGUID());
    return it != _lifebloomStacks.end() ? it->second : 0;
}

uint32 RestorationSpecialization::GetLifebloomStackTime(::Unit* target)
{
    if (!target)
        return 0;

    // This would need to track when Lifebloom was applied
    return getMSTime();
}

} // namespace Playerbot