/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PriestAI.h"
#include "ActionPriority.h"
#include "CooldownManager.h"
#include "ResourceManager.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Map.h"
#include "Group.h"
#include "Log.h"

namespace Playerbot
{

PriestAI::PriestAI(Player* bot) : ClassAI(bot), _currentRole(PriestRole::HEALER),
    _manaSpent(0), _healingDone(0), _damageDealt(0), _playersHealed(0), _damagePrevented(0),
    _lastGroupScan(0), _lastTriage(0), _groupAverageHealth(100.0f), _shadowOrbStacks(0),
    _mindBlastCooldown(0), _shadowformActive(false), _dotRefreshTimer(0),
    _powerWordShieldCharges(0), _penanceStacks(0), _lastDispel(0), _lastFearWard(0),
    _lastPsychicScream(0), _lastInnerFire(0)
{
    _specialization = DetectSpecialization();
    AdaptToGroupRole();

    TC_LOG_DEBUG("playerbot.priest", "PriestAI initialized for {} with specialization {} and role {}",
                 GetBot()->GetName(), static_cast<uint32>(_specialization), static_cast<uint32>(_currentRole));
}

void PriestAI::UpdateRotation(::Unit* target)
{
    // Priority 1: Healing (if in healing role or emergency)
    if (_currentRole == PriestRole::HEALER || IsEmergencyHealing())
    {
        UpdateHealingSystem();
    }

    // Priority 2: DPS rotation (if in DPS role or no healing needed)
    if (target && (_currentRole == PriestRole::DPS || !IsEmergencyHealing()))
    {
        switch (_specialization)
        {
            case PriestSpec::HOLY:
                UpdateHolyRotation(target);
                break;
            case PriestSpec::DISCIPLINE:
                UpdateDisciplineRotation(target);
                break;
            case PriestSpec::SHADOW:
                UpdateShadowRotation(target);
                break;
        }
    }

    // Priority 3: Utility and support
    ProvideUtilitySupport();
}

void PriestAI::UpdateBuffs()
{
    UpdatePriestBuffs();
}

void PriestAI::UpdateCooldowns(uint32 diff)
{
    // Update healing priorities and triage
    if (getMSTime() - _lastTriage > TRIAGE_INTERVAL)
    {
        PerformTriage();
        _lastTriage = getMSTime();
    }

    // Emergency healing response
    if (IsEmergencyHealing())
    {
        HandleEmergencyHealing();
    }

    // Use defensive abilities if in danger
    if (IsInDanger())
    {
        UseDefensiveAbilities();
    }

    // Manage specialization-specific mechanics
    switch (_specialization)
    {
        case PriestSpec::HOLY:
            ManageHolyPower();
            UpdateCircleOfHealing();
            break;
        case PriestSpec::DISCIPLINE:
            ManageDisciplineMechanics();
            UpdateShields();
            break;
        case PriestSpec::SHADOW:
            ManageShadowMechanics();
            UpdateDoTs();
            break;
    }

    // Mana management
    OptimizeManaUsage();

    // Role adaptation
    AdaptToGroupRole();
}

bool PriestAI::CanUseAbility(uint32 spellId)
{
    if (!IsSpellReady(spellId) || !IsSpellUsable(spellId))
        return false;

    if (!HasEnoughResource(spellId))
        return false;

    // Check if we're in the right form for shadow spells
    if (IsDamageSpell(spellId) && _specialization == PriestSpec::SHADOW)
    {
        if (!_shadowformActive && spellId != SHADOWFORM)
        {
            // Some shadow spells require shadowform
            return false;
        }
    }

    return true;
}

void PriestAI::OnCombatStart(::Unit* target)
{
    ClassAI::OnCombatStart(target);

    // Reset combat variables
    _manaSpent = 0;
    _healingDone = 0;
    _damageDealt = 0;
    _playersHealed = 0;

    // Determine role for this combat
    DetermineOptimalRole();

    // Prepare for combat based on specialization
    switch (_specialization)
    {
        case PriestSpec::SHADOW:
            if (!_shadowformActive)
                EnterShadowform();
            break;
        case PriestSpec::DISCIPLINE:
            // Pre-shield group members if possible
            break;
        case PriestSpec::HOLY:
            // Prepare for intensive healing
            break;
    }

    TC_LOG_DEBUG("playerbot.priest", "Priest {} entering combat - Spec: {}, Role: {}, Mana: {}%",
                 GetBot()->GetName(), static_cast<uint32>(_specialization),
                 static_cast<uint32>(_currentRole), GetManaPercent() * 100);
}

void PriestAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();

    // Analyze healing effectiveness
    AnalyzeHealingEfficiency();

    // Post-combat healing
    UpdateGroupHealing();

    // Use mana regeneration if needed
    if (GetManaPercent() < 0.4f)
    {
        UseManaRegeneration();
    }
}

bool PriestAI::HasEnoughResource(uint32 spellId)
{
    return _resourceManager->HasEnoughResource(spellId);
}

void PriestAI::ConsumeResource(uint32 spellId)
{
    uint32 manaCost = 0;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (spellInfo && spellInfo->PowerType == POWER_MANA)
    {
        manaCost = spellInfo->ManaCost + spellInfo->ManaCostPercentage * GetMaxMana() / 100;
    }

    _resourceManager->ConsumeResource(spellId);
    _manaSpent += manaCost;
}

Position PriestAI::GetOptimalPosition(::Unit* target)
{
    if (!target)
        return GetBot()->GetPosition();

    // Priests want to stay at safe healing range
    float distance = GetOptimalRange(target);
    float angle = GetBot()->GetAngle(target);

    // Stay back from combat, preferably with cover
    Position pos;
    target->GetNearPosition(pos, distance, angle + M_PI); // Behind target
    return pos;
}

float PriestAI::GetOptimalRange(::Unit* target)
{
    if (_currentRole == PriestRole::HEALER)
        return OPTIMAL_HEALING_RANGE;
    else
        return SAFE_HEALING_RANGE; // Closer for DPS but still safe
}

void PriestAI::UpdateHolyRotation(::Unit* target)
{
    if (!target)
        return;

    // Holy is primarily healing-focused, but can do some damage
    // 1. Smite for damage
    if (CanUseAbility(8092)) // Holy Fire
    {
        _actionQueue->AddAction(8092, ActionPriority::ROTATION, 70.0f, target);
    }
    else if (CanUseAbility(585)) // Smite
    {
        _actionQueue->AddAction(585, ActionPriority::ROTATION, 60.0f, target);
    }
}

void PriestAI::UpdateDisciplineRotation(::Unit* target)
{
    if (!target)
        return;

    // Discipline can use Penance for damage or healing
    if (CanUseAbility(PENANCE))
    {
        // Use on target for damage or on ally for healing
        ::Unit* healTarget = GetBestHealTarget();
        if (healTarget && healTarget->GetHealthPct() < 60.0f)
        {
            _actionQueue->AddAction(PENANCE, ActionPriority::ROTATION, 85.0f, healTarget);
        }
        else
        {
            _actionQueue->AddAction(PENANCE, ActionPriority::ROTATION, 75.0f, target);
        }
    }

    // Smite for Atonement healing
    if (CanUseAbility(585)) // Smite
    {
        _actionQueue->AddAction(585, ActionPriority::ROTATION, 65.0f, target);
    }
}

void PriestAI::UpdateShadowRotation(::Unit* target)
{
    if (!target)
        return;

    // Ensure we're in Shadowform
    if (!_shadowformActive && CanUseAbility(SHADOWFORM))
    {
        _actionQueue->AddAction(SHADOWFORM, ActionPriority::BUFF, 100.0f);
        return;
    }

    // Shadow priority rotation
    // 1. Shadow Word: Pain if not up
    if (!target->HasAura(SHADOW_WORD_PAIN) && CanUseAbility(SHADOW_WORD_PAIN))
    {
        _actionQueue->AddAction(SHADOW_WORD_PAIN, ActionPriority::ROTATION, 90.0f, target);
        return;
    }

    // 2. Vampiric Touch if not up
    if (!target->HasAura(VAMPIRIC_TOUCH) && CanUseAbility(VAMPIRIC_TOUCH))
    {
        _actionQueue->AddAction(VAMPIRIC_TOUCH, ActionPriority::ROTATION, 85.0f, target);
        return;
    }

    // 3. Devouring Plague if available
    if (!target->HasAura(DEVOURING_PLAGUE) && CanUseAbility(DEVOURING_PLAGUE))
    {
        _actionQueue->AddAction(DEVOURING_PLAGUE, ActionPriority::ROTATION, 80.0f, target);
        return;
    }

    // 4. Mind Blast on cooldown
    if (CanUseAbility(MIND_BLAST))
    {
        _actionQueue->AddAction(MIND_BLAST, ActionPriority::ROTATION, 75.0f, target);
        return;
    }

    // 5. Mind Flay as filler
    if (CanUseAbility(MIND_FLAY))
    {
        _actionQueue->AddAction(MIND_FLAY, ActionPriority::ROTATION, 60.0f, target);
    }
}

void PriestAI::UpdateHealingSystem()
{
    uint32 currentTime = getMSTime();

    // Scan for heal targets periodically
    if (currentTime - _lastGroupScan > HEAL_SCAN_INTERVAL)
    {
        ScanForHealTargets();
        _lastGroupScan = currentTime;
    }

    // Prioritize and execute healing
    PrioritizeHealing();
    ExecuteHealing();

    // Manage HoTs
    ManageHealOverTime();

    // Group healing if needed
    UpdateGroupHealing();
}

void PriestAI::ScanForHealTargets()
{
    // Clear previous queue
    while (!_healingQueue.empty())
        _healingQueue.pop();

    std::vector<::Unit*> potentialTargets;

    // Add self
    potentialTargets.push_back(GetBot());

    // Add group members
    if (Group* group = GetBot()->GetGroup())
    {
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                if (member != GetBot() && member->IsAlive())
                    potentialTargets.push_back(member);
            }
        }
    }

    // Add nearby allies (pets, NPCs, etc.)
    std::vector<::Unit*> nearbyAllies = GetNearbyEnemies(OPTIMAL_HEALING_RANGE); // Reuse function but for allies

    // Analyze each potential target
    for (::Unit* target : potentialTargets)
    {
        HealTarget healTarget = AnalyzeHealTarget(target);
        if (healTarget.priority != HealPriority::FULL)
        {
            _healingQueue.push(healTarget);
        }
    }

    TC_LOG_DEBUG("playerbot.priest", "Scanned for heal targets: {} targets need healing", _healingQueue.size());
}

void PriestAI::PrioritizeHealing()
{
    // The priority queue automatically prioritizes by HealPriority enum and health percentage
    // Additional logic for special situations

    if (_healingQueue.empty())
        return;

    // Calculate group average health for decision making
    float totalHealth = 0.0f;
    uint32 memberCount = 0;

    if (Group* group = GetBot()->GetGroup())
    {
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                totalHealth += member->GetHealthPct();
                memberCount++;
            }
        }
    }

    if (memberCount > 0)
        _groupAverageHealth = totalHealth / memberCount;
}

void PriestAI::ExecuteHealing()
{
    if (_healingQueue.empty())
        return;

    // Get highest priority heal target
    HealTarget healTarget = _healingQueue.top();
    _healingQueue.pop();

    // Validate target is still valid
    if (!healTarget.target || !healTarget.target->IsAlive())
        return;

    // Get optimal heal spell for this situation
    uint32 healSpell = GetOptimalHealSpell(healTarget);
    if (healSpell == 0)
        return;

    // Cast the healing spell
    CastHealingSpell(healSpell, healTarget.target);
}

HealTarget PriestAI::AnalyzeHealTarget(::Unit* target)
{
    if (!target || !target->IsAlive())
        return HealTarget();

    float healthPercent = target->GetHealthPct();
    uint32 missingHealth = target->GetMaxHealth() - target->GetHealth();
    HealPriority priority = CalculateHealPriority(target);

    HealTarget healTarget(target, priority, healthPercent, missingHealth);
    healTarget.inCombat = target->IsInCombat();
    healTarget.hasHoTs = TargetHasHoT(target, RENEW); // Check for existing HoTs
    healTarget.threatLevel = HasTooMuchThreat() ? 1.0f : 0.5f; // Simplified threat calc

    return healTarget;
}

HealPriority PriestAI::CalculateHealPriority(::Unit* target)
{
    if (!target)
        return HealPriority::FULL;

    float healthPercent = target->GetHealthPct();

    if (healthPercent < 25.0f)
        return HealPriority::EMERGENCY;
    else if (healthPercent < 50.0f)
        return HealPriority::CRITICAL;
    else if (healthPercent < 70.0f)
        return HealPriority::MODERATE;
    else if (healthPercent < 90.0f)
        return HealPriority::MAINTENANCE;
    else
        return HealPriority::FULL;
}

uint32 PriestAI::GetOptimalHealSpell(const HealTarget& healTarget)
{
    if (!healTarget.target)
        return 0;

    uint32 missingHealth = healTarget.missingHealth;
    bool conserveMana = ShouldConserveMana();

    // Emergency healing
    if (healTarget.priority == HealPriority::EMERGENCY)
    {
        if (CanUseAbility(FLASH_HEAL))
            return FLASH_HEAL;
        else if (CanUseAbility(HEAL))
            return HEAL;
    }

    // Efficient healing based on missing health
    if (missingHealth > 3000) // Large heal needed
    {
        if (!conserveMana && CanUseAbility(GREATER_HEAL))
            return GREATER_HEAL;
        else if (CanUseAbility(HEAL))
            return HEAL;
    }
    else if (missingHealth > 1500) // Medium heal needed
    {
        if (CanUseAbility(HEAL))
            return HEAL;
        else if (CanUseAbility(FLASH_HEAL))
            return FLASH_HEAL;
    }
    else // Small heal or maintenance
    {
        if (!healTarget.hasHoTs && CanUseAbility(RENEW))
            return RENEW;
        else if (CanUseAbility(FLASH_HEAL))
            return FLASH_HEAL;
    }

    return 0;
}

void PriestAI::CastHealingSpell(uint32 spellId, ::Unit* target)
{
    if (!spellId || !target || !CanUseAbility(spellId))
        return;

    // Check range
    if (!IsInRange(target, spellId))
        return;

    // Add to action queue with high priority
    ActionPriority priority = ActionPriority::SURVIVAL;
    if (target->GetHealthPct() < 25.0f)
        priority = ActionPriority::EMERGENCY;

    float score = 100.0f - target->GetHealthPct(); // Lower health = higher score
    _actionQueue->AddAction(spellId, priority, score, target);

    TC_LOG_DEBUG("playerbot.priest", "Queued heal spell {} for {} ({}% health)",
                 spellId, target->GetName(), target->GetHealthPct());
}

void PriestAI::PerformTriage()
{
    // Quick scan for emergency situations
    if (Group* group = GetBot()->GetGroup())
    {
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                if (member->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD)
                {
                    // Emergency healing needed
                    HealTarget emergency(member, HealPriority::EMERGENCY, member->GetHealthPct(),
                                       member->GetMaxHealth() - member->GetHealth());
                    _healingQueue.push(emergency);
                }
            }
        }
    }
}

void PriestAI::HandleEmergencyHealing()
{
    // Use most powerful emergency heals
    ::Unit* criticalTarget = GetHighestPriorityPatient();
    if (!criticalTarget)
        return;

    // Guardian Spirit for near-death
    if (criticalTarget->GetHealthPct() < 10.0f && CanUseAbility(GUARDIAN_SPIRIT))
    {
        _actionQueue->AddAction(GUARDIAN_SPIRIT, ActionPriority::EMERGENCY, 100.0f, criticalTarget);
        return;
    }

    // Flash Heal for speed
    if (CanUseAbility(FLASH_HEAL))
    {
        _actionQueue->AddAction(FLASH_HEAL, ActionPriority::EMERGENCY, 95.0f, criticalTarget);
    }
}

bool PriestAI::IsEmergencyHealing()
{
    // Check if anyone is in critical condition
    if (Group* group = GetBot()->GetGroup())
    {
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                if (member->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD)
                    return true;
            }
        }
    }

    return GetBot()->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD;
}

void PriestAI::PrioritizeEmergencyTargets()
{
    // Move emergency targets to front of queue
    // This is handled by the priority queue automatically
}

void PriestAI::UpdateGroupHealing()
{
    if (!GetBot()->GetGroup())
        return;

    // Check if group healing is efficient
    uint32 injuredMembers = 0;
    uint32 totalMembers = 0;

    if (Group* group = GetBot()->GetGroup())
    {
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                totalMembers++;
                if (member->GetHealthPct() < 80.0f)
                    injuredMembers++;
            }
        }
    }

    // Use group heal if 3+ members need healing
    if (injuredMembers >= 3)
    {
        CastGroupHeal();
    }
}

void PriestAI::CastGroupHeal()
{
    // Prayer of Healing for group heal
    if (CanUseAbility(PRAYER_OF_HEALING))
    {
        _actionQueue->AddAction(PRAYER_OF_HEALING, ActionPriority::SURVIVAL, 80.0f);
    }
    // Circle of Healing if available
    else if (CanUseAbility(CIRCLE_OF_HEALING))
    {
        _actionQueue->AddAction(CIRCLE_OF_HEALING, ActionPriority::SURVIVAL, 85.0f);
    }
}

void PriestAI::ManageHealOverTime()
{
    // Refresh Renew on targets that need it
    std::vector<::Unit*> targets;
    targets.push_back(GetBot());

    if (Group* group = GetBot()->GetGroup())
    {
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                if (member != GetBot())
                    targets.push_back(member);
            }
        }
    }

    for (::Unit* target : targets)
    {
        if (target->GetHealthPct() < 90.0f && !TargetHasHoT(target, RENEW))
        {
            if (CanUseAbility(RENEW))
            {
                _actionQueue->AddAction(RENEW, ActionPriority::BUFF, 70.0f, target);
            }
        }
    }
}

void PriestAI::OptimizeGroupHealEfficiency()
{
    // Analyze which healing spells are most efficient for group situations
}

void PriestAI::UpdatePriestBuffs()
{
    // Inner Fire for self
    CastInnerFire();

    // Fortitude buffs for group
    UpdateFortitudeBuffs();

    // Spirit buffs if available
    if (!HasAura(DIVINE_SPIRIT) && CanUseAbility(DIVINE_SPIRIT))
    {
        CastSpell(DIVINE_SPIRIT);
    }

    // Fear Ward on tank if available
    if (CanUseAbility(FEAR_WARD))
    {
        ::Unit* tank = GetLowestHealthAlly(); // Simplified tank detection
        if (tank && !tank->HasAura(FEAR_WARD))
        {
            _actionQueue->AddAction(FEAR_WARD, ActionPriority::BUFF, 60.0f, tank);
        }
    }
}

void PriestAI::CastInnerFire()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastInnerFire > 600000 && // 10 minutes duration
        !HasAura(INNER_FIRE) && CanUseAbility(INNER_FIRE))
    {
        if (CastSpell(INNER_FIRE))
        {
            _lastInnerFire = currentTime;
        }
    }
}

void PriestAI::UpdateFortitudeBuffs()
{
    // Self fortitude
    if (!HasAura(POWER_WORD_FORTITUDE) && CanUseAbility(POWER_WORD_FORTITUDE))
    {
        CastSpell(POWER_WORD_FORTITUDE);
    }

    // Group fortitude
    if (GetBot()->GetGroup() && CanUseAbility(PRAYER_OF_FORTITUDE))
    {
        bool needsGroupBuff = false;
        if (Group* group = GetBot()->GetGroup())
        {
            for (GroupReference const& ref : group->GetMembers())
            {
                if (Player* member = ref.GetSource())
                {
                    if (!member->HasAura(POWER_WORD_FORTITUDE) && !member->HasAura(PRAYER_OF_FORTITUDE))
                    {
                        needsGroupBuff = true;
                        break;
                    }
                }
            }
        }

        if (needsGroupBuff)
        {
            _actionQueue->AddAction(PRAYER_OF_FORTITUDE, ActionPriority::BUFF, 50.0f);
        }
    }
}

void PriestAI::UseDisciplineAbilities(::Unit* target)
{
    if (!target)
        return;

    // Power Word: Shield on targets that need it
    if (target->GetHealthPct() < 70.0f && !target->HasAura(POWER_WORD_SHIELD))
    {
        CastPowerWordShield(target);
    }

    // Pain Suppression for emergency
    if (target->GetHealthPct() < 20.0f && CanUseAbility(PAIN_SUPPRESSION))
    {
        _actionQueue->AddAction(PAIN_SUPPRESSION, ActionPriority::EMERGENCY, 100.0f, target);
    }
}

void PriestAI::CastPowerWordShield(::Unit* target)
{
    if (!target || !CanUseAbility(POWER_WORD_SHIELD))
        return;

    _actionQueue->AddAction(POWER_WORD_SHIELD, ActionPriority::SURVIVAL, 80.0f, target);
}

void PriestAI::CastPenance(::Unit* target)
{
    if (!target || !CanUseAbility(PENANCE))
        return;

    float score = 75.0f;
    ActionPriority priority = ActionPriority::ROTATION;

    // Higher priority for healing
    if (target->GetHealthPct() < 50.0f)
    {
        priority = ActionPriority::SURVIVAL;
        score = 90.0f;
    }

    _actionQueue->AddAction(PENANCE, priority, score, target);
}

void PriestAI::UseShadowAbilities(::Unit* target)
{
    if (!target)
        return;

    // Ensure Shadowform is active
    if (!_shadowformActive)
    {
        EnterShadowform();
        return;
    }

    // Apply DoTs
    CastShadowWordPain(target);
    CastVampiricTouch(target);
    CastDevouringPlague(target);

    // Use direct damage
    CastMindBlast(target);
    CastMindFlay(target);
}

void PriestAI::CastShadowWordPain(::Unit* target)
{
    if (!target || target->HasAura(SHADOW_WORD_PAIN) || !CanUseAbility(SHADOW_WORD_PAIN))
        return;

    _actionQueue->AddAction(SHADOW_WORD_PAIN, ActionPriority::ROTATION, 85.0f, target);
}

void PriestAI::CastVampiricTouch(::Unit* target)
{
    if (!target || target->HasAura(VAMPIRIC_TOUCH) || !CanUseAbility(VAMPIRIC_TOUCH))
        return;

    _actionQueue->AddAction(VAMPIRIC_TOUCH, ActionPriority::ROTATION, 80.0f, target);
}

void PriestAI::CastDevouringPlague(::Unit* target)
{
    if (!target || target->HasAura(DEVOURING_PLAGUE) || !CanUseAbility(DEVOURING_PLAGUE))
        return;

    _actionQueue->AddAction(DEVOURING_PLAGUE, ActionPriority::ROTATION, 75.0f, target);
}

void PriestAI::CastMindBlast(::Unit* target)
{
    if (!target || !CanUseAbility(MIND_BLAST))
        return;

    _actionQueue->AddAction(MIND_BLAST, ActionPriority::ROTATION, 70.0f, target);
}

void PriestAI::CastMindFlay(::Unit* target)
{
    if (!target || !CanUseAbility(MIND_FLAY))
        return;

    _actionQueue->AddAction(MIND_FLAY, ActionPriority::ROTATION, 60.0f, target);
}

void PriestAI::EnterShadowform()
{
    if (CanUseAbility(SHADOWFORM))
    {
        _actionQueue->AddAction(SHADOWFORM, ActionPriority::BUFF, 100.0f);
        _shadowformActive = true;
    }
}

void PriestAI::ExitShadowform()
{
    if (_shadowformActive && HasAura(SHADOWFORM))
    {
        // Cancel shadowform by casting it again or using a cancel aura method
        _shadowformActive = false;
    }
}

uint32 PriestAI::GetMana()
{
    return _resourceManager->GetResource(ResourceType::MANA);
}

uint32 PriestAI::GetMaxMana()
{
    return _resourceManager->GetMaxResource(ResourceType::MANA);
}

float PriestAI::GetManaPercent()
{
    return _resourceManager->GetResourcePercent(ResourceType::MANA);
}

void PriestAI::OptimizeManaUsage()
{
    float manaPercent = GetManaPercent();

    if (manaPercent < MANA_CONSERVATION_THRESHOLD)
    {
        // Use more efficient heals
        // Prioritize HoTs over direct heals
        // Use Hymn of Hope if available
        if (CanUseAbility(HYMN_OF_HOPE))
        {
            _actionQueue->AddAction(HYMN_OF_HOPE, ActionPriority::SURVIVAL, 90.0f);
        }
    }
}

bool PriestAI::ShouldConserveMana()
{
    return GetManaPercent() < MANA_CONSERVATION_THRESHOLD;
}

void PriestAI::UseManaRegeneration()
{
    // Hymn of Hope for group mana regen
    if (CanUseAbility(HYMN_OF_HOPE))
    {
        CastSpell(HYMN_OF_HOPE);
    }
}

void PriestAI::CastHymnOfHope()
{
    if (CanUseAbility(HYMN_OF_HOPE))
    {
        _actionQueue->AddAction(HYMN_OF_HOPE, ActionPriority::SURVIVAL, 80.0f);
    }
}

void PriestAI::UseDefensiveAbilities()
{
    // Psychic Scream to fear nearby enemies
    if (GetEnemyCount(8.0f) > 0 && CanUseAbility(PSYCHIC_SCREAM))
    {
        CastPsychicScream();
    }

    // Fade to reduce threat
    if (HasTooMuchThreat() && CanUseAbility(FADE))
    {
        CastFade();
    }

    // Shield self if low health
    if (GetBot()->GetHealthPct() < 40.0f && CanUseAbility(POWER_WORD_SHIELD))
    {
        CastPowerWordShield(GetBot());
    }
}

void PriestAI::CastPsychicScream()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastPsychicScream > 30000 && CanUseAbility(PSYCHIC_SCREAM))
    {
        _actionQueue->AddAction(PSYCHIC_SCREAM, ActionPriority::SURVIVAL, 85.0f);
        _lastPsychicScream = currentTime;
    }
}

void PriestAI::CastFade()
{
    if (CanUseAbility(FADE))
    {
        _actionQueue->AddAction(FADE, ActionPriority::SURVIVAL, 80.0f);
    }
}

::Unit* PriestAI::GetBestHealTarget()
{
    ::Unit* bestTarget = nullptr;
    float lowestHealthPct = 100.0f;

    // Check self
    if (GetBot()->GetHealthPct() < lowestHealthPct)
    {
        bestTarget = GetBot();
        lowestHealthPct = GetBot()->GetHealthPct();
    }

    // Check group members
    if (Group* group = GetBot()->GetGroup())
    {
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                if (member != GetBot() && member->GetHealthPct() < lowestHealthPct)
                {
                    bestTarget = member;
                    lowestHealthPct = member->GetHealthPct();
                }
            }
        }
    }

    return bestTarget;
}

::Unit* PriestAI::GetHighestPriorityPatient()
{
    ::Unit* criticalTarget = nullptr;
    float lowestHealthPct = EMERGENCY_HEALTH_THRESHOLD;

    // Find most critical target
    if (Group* group = GetBot()->GetGroup())
    {
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                if (member->GetHealthPct() < lowestHealthPct)
                {
                    criticalTarget = member;
                    lowestHealthPct = member->GetHealthPct();
                }
            }
        }
    }

    if (GetBot()->GetHealthPct() < lowestHealthPct)
    {
        criticalTarget = GetBot();
    }

    return criticalTarget;
}

void PriestAI::ProvideUtilitySupport()
{
    // Dispel magic debuffs
    UpdateDispelling();

    // Assist with crowd control if needed
    // Mind Control dangerous enemies
    // Shackle undead
}

void PriestAI::UpdateDispelling()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastDispel < DISPEL_COOLDOWN)
        return;

    ::Unit* dispelTarget = GetBestDispelTarget();
    if (dispelTarget && CanUseAbility(DISPEL_MAGIC))
    {
        _actionQueue->AddAction(DISPEL_MAGIC, ActionPriority::SURVIVAL, 75.0f, dispelTarget);
        _lastDispel = currentTime;
    }
}

::Unit* PriestAI::GetBestDispelTarget()
{
    // Check group members for dispellable debuffs
    if (Group* group = GetBot()->GetGroup())
    {
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                // This would check for dispellable auras
                // For now, return first group member as placeholder
                return member;
            }
        }
    }

    return nullptr;
}

void PriestAI::AdaptToGroupRole()
{
    // Determine role based on group composition and situation
    DetermineOptimalRole();
}

void PriestAI::DetermineOptimalRole()
{
    // Simple role determination logic
    if (_specialization == PriestSpec::SHADOW)
    {
        _currentRole = PriestRole::DPS;
    }
    else if (GetBot()->GetGroup())
    {
        // In group, prioritize healing
        _currentRole = PriestRole::HEALER;
    }
    else
    {
        // Solo, be hybrid
        _currentRole = PriestRole::HYBRID;
    }
}

bool PriestAI::IsHealingSpell(uint32 spellId)
{
    return spellId == HEAL || spellId == GREATER_HEAL || spellId == FLASH_HEAL ||
           spellId == RENEW || spellId == PRAYER_OF_HEALING || spellId == CIRCLE_OF_HEALING ||
           spellId == BINDING_HEAL || spellId == PENANCE;
}

bool PriestAI::IsDamageSpell(uint32 spellId)
{
    return spellId == SHADOW_WORD_PAIN || spellId == MIND_BLAST || spellId == MIND_FLAY ||
           spellId == VAMPIRIC_TOUCH || spellId == DEVOURING_PLAGUE || spellId == SHADOW_WORD_DEATH;
}

bool PriestAI::TargetHasHoT(::Unit* target, uint32 spellId)
{
    return target ? target->HasAura(spellId) : false;
}

PriestSpec PriestAI::DetectSpecialization()
{
    // This would detect the priest's specialization based on talents
    // For now, return a default
    return PriestSpec::HOLY; // Default to Holy for healing capability
}

bool PriestAI::IsInDanger()
{
    float healthPct = GetBot()->GetHealthPct();
    uint32 nearbyEnemies = GetEnemyCount(10.0f);

    return healthPct < 50.0f || nearbyEnemies > 1;
}

bool PriestAI::HasTooMuchThreat()
{
    // Simplified threat check
    return false; // Placeholder
}

void PriestAI::RecordHealingDone(uint32 amount, ::Unit* target)
{
    _healingDone += amount;
    _playersHealed++;
    RecordPerformanceMetric("healing_done", amount);
}

void PriestAI::AnalyzeHealingEfficiency()
{
    if (_manaSpent > 0)
    {
        float efficiency = static_cast<float>(_healingDone) / _manaSpent;
        TC_LOG_DEBUG("playerbot.priest", "Healing efficiency: {} healing per mana", efficiency);
    }

    RecordPerformanceMetric("players_healed", _playersHealed);
    RecordPerformanceMetric("mana_spent", _manaSpent);
}

// Placeholder implementations for complex methods
void PriestAI::ManageHolyPower() {}
void PriestAI::UpdateCircleOfHealing() {}
void PriestAI::ManageSerendipity() {}
void PriestAI::ManageDisciplineMechanics() {}
void PriestAI::UpdateBorrowedTime() {}
void PriestAI::ManageGrace() {}
void PriestAI::ManageShadowMechanics() {}
void PriestAI::UpdateShadowWeaving() {}
void PriestAI::ManageVampiricEmbrace() {}
void PriestAI::ManageAtonement() {}
void PriestAI::UpdateShields() {}
void PriestAI::ManageShadowOrbs() {}
void PriestAI::UpdateDoTs() {}
void PriestAI::CastDivineFavor() {}
void PriestAI::CastSpiritOfRedemption() {}
void PriestAI::CastPowerWordFortitude() {}
void PriestAI::CastPrayerOfFortitude() {}
void PriestAI::CastDispelMagic() {}
void PriestAI::CastFearWard() {}
void PriestAI::UseShadowProtection() {}
void PriestAI::UseCrowdControl(::Unit* target) {}
void PriestAI::CastMindControl(::Unit* target) {}
void PriestAI::CastShackleUndead(::Unit* target) {}
void PriestAI::CastSilence(::Unit* target) {}
void PriestAI::UpdatePriestPositioning() {}
bool PriestAI::IsAtOptimalHealingRange(::Unit* target) { return true; }
void PriestAI::MaintainHealingPosition() {}
void PriestAI::FindSafePosition() {}
::Unit* PriestAI::GetBestDispelTarget() { return nullptr; }
::Unit* PriestAI::GetBestMindControlTarget() { return nullptr; }
::Unit* PriestAI::GetLowestHealthAlly() { return GetBestHealTarget(); }
void PriestAI::CheckForDebuffs() {}
void PriestAI::AssistGroupMembers() {}
void PriestAI::SwitchToHealingRole() { _currentRole = PriestRole::HEALER; }
void PriestAI::SwitchToDamageRole() { _currentRole = PriestRole::DPS; }
void PriestAI::ManageThreat() {}
void PriestAI::ReduceThreat() { CastFade(); }
void PriestAI::UseFade() { CastFade(); }
void PriestAI::RecordDamageDone(uint32 amount, ::Unit* target) { _damageDealt += amount; }
void PriestAI::OptimizeHealingRotation() {}
uint32 PriestAI::GetSpellHealAmount(uint32 spellId) { return 1000; } // Placeholder
uint32 PriestAI::GetHealOverTimeRemaining(::Unit* target, uint32 spellId) { return 0; }
void PriestAI::OptimizeForSpecialization() {}
bool PriestAI::HasTalent(uint32 talentId) { return false; }
bool PriestAI::HasEnoughMana(uint32 amount) { return GetMana() >= amount; }

// PriestHealCalculator implementation
std::unordered_map<uint32, uint32> PriestHealCalculator::_baseHealCache;
std::unordered_map<uint32, float> PriestHealCalculator::_efficiencyCache;
std::mutex PriestHealCalculator::_cacheMutex;

uint32 PriestHealCalculator::CalculateHealAmount(uint32 spellId, Player* caster, ::Unit* target)
{
    // Placeholder implementation
    return 1000;
}

uint32 PriestHealCalculator::CalculateHealOverTime(uint32 spellId, Player* caster)
{
    // Placeholder implementation
    return 500;
}

float PriestHealCalculator::CalculateHealEfficiency(uint32 spellId, Player* caster)
{
    // Placeholder implementation
    return 1.0f;
}

float PriestHealCalculator::CalculateHealPerMana(uint32 spellId, Player* caster)
{
    // Placeholder implementation
    return 2.0f;
}

uint32 PriestHealCalculator::GetOptimalHealForSituation(Player* caster, ::Unit* target, uint32 missingHealth)
{
    // Placeholder implementation
    return PriestAI::HEAL;
}

bool PriestHealCalculator::ShouldUseDirectHeal(Player* caster, ::Unit* target)
{
    return target ? target->GetHealthPct() < 50.0f : false;
}

bool PriestHealCalculator::ShouldUseHealOverTime(Player* caster, ::Unit* target)
{
    return target ? target->GetHealthPct() > 50.0f && target->GetHealthPct() < 90.0f : false;
}

bool PriestHealCalculator::ShouldUseGroupHeal(Player* caster, const std::vector<::Unit*>& targets)
{
    return targets.size() > 2;
}

float PriestHealCalculator::CalculateHealThreat(uint32 healAmount, Player* caster)
{
    return healAmount * 0.5f; // Simplified threat calculation
}

bool PriestHealCalculator::WillOverheal(uint32 spellId, Player* caster, ::Unit* target)
{
    return false; // Placeholder
}

void PriestHealCalculator::CacheHealData(uint32 spellId)
{
    // Placeholder implementation
}

} // namespace Playerbot