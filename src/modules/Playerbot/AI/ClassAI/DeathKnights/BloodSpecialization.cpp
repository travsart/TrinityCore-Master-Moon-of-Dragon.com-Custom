/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "BloodSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Object.h"
#include "Creature.h"

namespace Playerbot
{

BloodSpecialization::BloodSpecialization(Player* bot)
    : DeathKnightSpecialization(bot)
    , _lastThreatUpdate(0)
    , _vampiricBloodReady(0)
    , _boneShieldStacks(0)
    , _dancingRuneWeaponReady(0)
    , _iceboundFortitudeReady(0)
    , _antiMagicShellReady(0)
    , _lastVampiricBlood(0)
    , _lastBoneShield(0)
    , _lastDancingRuneWeapon(0)
    , _lastIceboundFortitude(0)
    , _lastAntiMagicShell(0)
    , _lastSelfHeal(0)
    , _healingReceived(0)
    , _damageAbsorbed(0)
    , _totalThreatGenerated(0)
    , _totalSelfHealing(0)
    , _runicPowerSpent(0)
{
}

void BloodSpecialization::UpdateRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (!target->IsHostileTo(bot))
        return;

    UpdateRuneManagement();
    UpdateRunicPowerManagement();
    UpdateThreatManagement();
    UpdateSelfHealing();
    UpdateDiseaseManagement();
    UpdateDefensiveCooldowns();

    // Emergency abilities
    if (bot->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD * 100)
    {
        ManageEmergencyAbilities();
        return;
    }

    // Ensure we're in Blood Presence
    if (ShouldUseBloodPresence())
    {
        EnterBloodPresence();
    }

    // Maintain Bone Shield
    if (ShouldCastBoneShield())
    {
        CastBoneShield();
        return;
    }

    // Use defensive cooldowns when needed
    if (bot->GetHealthPct() < 60.0f)
    {
        UseDefensiveCooldowns();
    }

    // Self-healing priority
    if (ShouldSelfHeal() && ShouldCastDeathStrike(target))
    {
        CastDeathStrike(target);
        return;
    }

    // Disease application priority
    if (ShouldApplyDisease(target, DiseaseType::BLOOD_PLAGUE))
    {
        CastPlagueStrike(target);
        return;
    }

    // Threat generation for multiple targets
    std::vector<::Unit*> threatTargets = GetThreatTargets();
    if (threatTargets.size() > 1)
    {
        if (ShouldCastBloodBoil())
        {
            CastBloodBoil();
            return;
        }

        if (ShouldCastDeathAndDecay())
        {
            CastDeathAndDecay(target->GetPosition());
            return;
        }
    }

    // Single target rotation
    if (ShouldCastHeartStrike(target))
    {
        CastHeartStrike(target);
        return;
    }

    if (ShouldCastDeathStrike(target))
    {
        CastDeathStrike(target);
        return;
    }

    // Use Death Grip for positioning
    if (ShouldUseDeathGrip(target))
    {
        CastDeathGrip(target);
        return;
    }

    // Ranged abilities
    if (bot->GetDistance(target) > BLOOD_MELEE_RANGE)
    {
        CastDeathCoil(target);
    }
}

void BloodSpecialization::UpdateBuffs()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Maintain Blood Presence
    if (!bot->HasAura(BLOOD_PRESENCE) && bot->HasSpell(BLOOD_PRESENCE))
    {
        bot->CastSpell(bot, BLOOD_PRESENCE, false);
    }

    // Maintain Bone Shield
    if (ShouldCastBoneShield())
    {
        CastBoneShield();
    }

    // Maintain Horn of Winter
    if (!bot->HasAura(HORN_OF_WINTER) && bot->HasSpell(HORN_OF_WINTER))
    {
        bot->CastSpell(bot, HORN_OF_WINTER, false);
    }
}

void BloodSpecialization::UpdateCooldowns(uint32 diff)
{
    for (auto& cooldown : _cooldowns)
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;

    if (_vampiricBloodReady > diff)
        _vampiricBloodReady -= diff;
    else
        _vampiricBloodReady = 0;

    if (_dancingRuneWeaponReady > diff)
        _dancingRuneWeaponReady -= diff;
    else
        _dancingRuneWeaponReady = 0;

    if (_iceboundFortitudeReady > diff)
        _iceboundFortitudeReady -= diff;
    else
        _iceboundFortitudeReady = 0;

    if (_antiMagicShellReady > diff)
        _antiMagicShellReady -= diff;
    else
        _antiMagicShellReady = 0;

    if (_lastThreatUpdate > diff)
        _lastThreatUpdate -= diff;
    else
        _lastThreatUpdate = 0;

    RegenerateRunes(diff);
    UpdateDiseaseTimers(diff);
}

bool BloodSpecialization::CanUseAbility(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    return HasEnoughResource(spellId);
}

void BloodSpecialization::OnCombatStart(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Enter Blood Presence
    if (ShouldUseBloodPresence())
        EnterBloodPresence();

    // Apply initial defensive buffs
    if (ShouldCastBoneShield())
        CastBoneShield();
}

void BloodSpecialization::OnCombatEnd()
{
    _threatTargets.clear();
    _cooldowns.clear();
    // _activeDiseases is a protected member, cleared via virtual method
    _boneShieldStacks = 0;
}

bool BloodSpecialization::HasEnoughResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    switch (spellId)
    {
        case DEATH_STRIKE:
            return HasAvailableRunes(RuneType::FROST, 1) && HasAvailableRunes(RuneType::UNHOLY, 1);
        case HEART_STRIKE:
            return HasAvailableRunes(RuneType::BLOOD, 1);
        case BLOOD_BOIL:
            return HasAvailableRunes(RuneType::BLOOD, 1);
        case PLAGUE_STRIKE:
            return HasAvailableRunes(RuneType::UNHOLY, 1);
        case DEATH_COIL:
            return HasEnoughRunicPower(40);
        case VAMPIRIC_BLOOD:
            return _vampiricBloodReady == 0;
        case BONE_SHIELD:
            return HasAvailableRunes(RuneType::UNHOLY, 1);
        case DANCING_RUNE_WEAPON:
            return _dancingRuneWeaponReady == 0;
        case ICEBOUND_FORTITUDE:
            return _iceboundFortitudeReady == 0;
        case ANTI_MAGIC_SHELL:
            return _antiMagicShellReady == 0;
        case DEATH_AND_DECAY:
            return HasAvailableRunes(RuneType::BLOOD, 1) && HasAvailableRunes(RuneType::FROST, 1) && HasAvailableRunes(RuneType::UNHOLY, 1);
        default:
            return true;
    }
}

void BloodSpecialization::ConsumeResource(uint32 spellId)
{
    switch (spellId)
    {
        case DEATH_STRIKE:
            ConsumeRunes(RuneType::FROST, 1);
            ConsumeRunes(RuneType::UNHOLY, 1);
            GenerateRunicPower(15);
            break;
        case HEART_STRIKE:
            ConsumeRunes(RuneType::BLOOD, 1);
            GenerateRunicPower(10);
            break;
        case BLOOD_BOIL:
            ConsumeRunes(RuneType::BLOOD, 1);
            GenerateRunicPower(10);
            break;
        case PLAGUE_STRIKE:
            ConsumeRunes(RuneType::UNHOLY, 1);
            GenerateRunicPower(10);
            break;
        case DEATH_COIL:
            SpendRunicPower(40);
            break;
        case VAMPIRIC_BLOOD:
            _vampiricBloodReady = VAMPIRIC_BLOOD_COOLDOWN;
            _lastVampiricBlood = getMSTime();
            break;
        case BONE_SHIELD:
            ConsumeRunes(RuneType::UNHOLY, 1);
            _boneShieldStacks = BONE_SHIELD_MAX_STACKS;
            _lastBoneShield = getMSTime();
            break;
        case DANCING_RUNE_WEAPON:
            _dancingRuneWeaponReady = DANCING_RUNE_WEAPON_COOLDOWN;
            _lastDancingRuneWeapon = getMSTime();
            break;
        case ICEBOUND_FORTITUDE:
            _iceboundFortitudeReady = ICEBOUND_FORTITUDE_COOLDOWN;
            _lastIceboundFortitude = getMSTime();
            break;
        case ANTI_MAGIC_SHELL:
            _antiMagicShellReady = ANTI_MAGIC_SHELL_COOLDOWN;
            _lastAntiMagicShell = getMSTime();
            break;
        case DEATH_AND_DECAY:
            ConsumeRunes(RuneType::BLOOD, 1);
            ConsumeRunes(RuneType::FROST, 1);
            ConsumeRunes(RuneType::UNHOLY, 1);
            break;
        default:
            break;
    }
}

Position BloodSpecialization::GetOptimalPosition(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return Position();

    // Tank stays in front of target
    float distance = BLOOD_MELEE_RANGE * 0.8f;
    float angle = target->GetAbsoluteAngle(bot->GetPositionX(), bot->GetPositionY());

    return Position(
        target->GetPositionX() + distance * cos(angle),
        target->GetPositionY() + distance * sin(angle),
        target->GetPositionZ(),
        target->GetAbsoluteAngle(bot->GetPositionX(), bot->GetPositionY()) + M_PI
    );
}

float BloodSpecialization::GetOptimalRange(::Unit* target)
{
    return BLOOD_MELEE_RANGE;
}

// Implementation of pure virtual methods from base class
void BloodSpecialization::UpdateRuneManagement()
{
    RegenerateRunes(0);
}

bool BloodSpecialization::HasAvailableRunes(RuneType type, uint32 count)
{
    return GetAvailableRunes(type) >= count;
}

void BloodSpecialization::ConsumeRunes(RuneType type, uint32 count)
{
    // Implementation deferred to base class method
    // TODO: Implement proper rune consumption via protected interface
    RegenerateRunes(0);
}

uint32 BloodSpecialization::GetAvailableRunes(RuneType type) const
{
    // Simplified implementation - use base class methods
    // TODO: Implement proper rune counting via protected interface
    return GetTotalAvailableRunes() > 0 ? 1 : 0; // Conservative approach
}

void BloodSpecialization::UpdateRunicPowerManagement()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Decay runic power out of combat
    if (!bot->IsInCombat())
    {
        uint32 now = getMSTime();
        // Runic power decay is managed by base class
        // Direct access to protected members is not allowed
    }
}

void BloodSpecialization::GenerateRunicPower(uint32 amount)
{
    // Implementation deferred to base class
    // TODO: Add protected method for runic power generation
}

void BloodSpecialization::SpendRunicPower(uint32 amount)
{
    // Implementation deferred to base class
    // TODO: Add protected method for runic power spending
}

uint32 BloodSpecialization::GetRunicPower() const
{
    // Return 0 as placeholder - needs base class protected method
    return 0;
}

bool BloodSpecialization::HasEnoughRunicPower(uint32 required) const
{
    // Conservative approach - needs base class protected method
    return true; // Placeholder
}

void BloodSpecialization::UpdateDiseaseManagement()
{
    UpdateDiseaseTimers(0);
    RefreshExpringDiseases();
}

void BloodSpecialization::ApplyDisease(::Unit* target, DiseaseType type, uint32 spellId)
{
    if (!target)
        return;

    DiseaseInfo disease(type, spellId, 15000); // 15 seconds duration
    // Use protected method to apply disease instead of direct access
}

bool BloodSpecialization::HasDisease(::Unit* target, DiseaseType type) const
{
    if (!target)
        return false;

    auto diseases = GetActiveDiseases(target);
    for (const auto& disease : diseases)
    {
        if (disease.type == type && disease.remainingTime > 0)
            return true;
    }

    return false;
}

bool BloodSpecialization::ShouldApplyDisease(::Unit* target, DiseaseType type) const
{
    if (!target)
        return false;

    return !HasDisease(target, type) || GetDiseaseRemainingTime(target, type) < 6000; // 6 seconds threshold
}

void BloodSpecialization::RefreshExpringDiseases()
{
    // Blood specialization focuses on Blood Plague
    // Disease refresh handled via base class protected methods
    // TODO: Implement disease refresh logic via proper interface
}

void BloodSpecialization::UpdateDeathAndDecay()
{
    // Death and Decay management is handled in main rotation
}

bool BloodSpecialization::ShouldCastDeathAndDecay() const
{
    return GetThreatTargets().size() > 2; // Simplified - needs proper cooldown tracking
}

void BloodSpecialization::CastDeathAndDecay(Position targetPos)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(DEATH_AND_DECAY))
    {
        bot->CastSpell(bot, DEATH_AND_DECAY, false);
        ConsumeResource(DEATH_AND_DECAY);
        // Death and Decay positioning managed by base class
        // TODO: Add protected methods for death and decay tracking
    }
}

void BloodSpecialization::UpdateThreatManagement()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    uint32 now = getMSTime();
    if (now - _lastThreatUpdate < THREAT_UPDATE_INTERVAL)
        return;

    _threatTargets.clear();

    // Find all hostile targets in threat range
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference const& itr : group->GetMembers())
        {
            Player* member = itr.GetSource();
            if (!member || !member->IsInWorld())
                continue;

            // Check for enemies targeting group members
            for (auto const& [guid, threatRef] : member->GetThreatManager().GetThreatenedByMeList())
            {
                if (Creature* creature = threatRef->GetOwner())
                {
                    if (creature->IsWithinDistInMap(bot, 30.0f))
                        _threatTargets.push_back(static_cast<Unit*>(creature));
                }
            }
        }
    }

    _lastThreatUpdate = now;
}

void BloodSpecialization::UpdateSelfHealing()
{
    ManageSelfHealing();
}

void BloodSpecialization::UpdateDefensiveCooldowns()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Check if we need Vampiric Blood
    if (bot->GetHealthPct() < 50.0f && ShouldCastVampiricBlood())
    {
        CastVampiricBlood();
    }

    // Check if we need Dancing Rune Weapon
    if (bot->GetHealthPct() < 40.0f && ShouldCastDancingRuneWeapon())
    {
        CastDancingRuneWeapon();
    }
}

bool BloodSpecialization::ShouldCastDeathStrike(::Unit* target)
{
    return target && GetBot()->IsWithinMeleeRange(target) &&
           HasEnoughResource(DEATH_STRIKE) && ShouldSelfHeal();
}

bool BloodSpecialization::ShouldCastHeartStrike(::Unit* target)
{
    return target && GetBot()->IsWithinMeleeRange(target) &&
           HasEnoughResource(HEART_STRIKE);
}

bool BloodSpecialization::ShouldCastBloodBoil()
{
    return HasEnoughResource(BLOOD_BOIL) && GetThreatTargets().size() > 1;
}

bool BloodSpecialization::ShouldCastVampiricBlood()
{
    return _vampiricBloodReady == 0 && GetBot()->GetHealthPct() < 60.0f;
}

bool BloodSpecialization::ShouldCastBoneShield()
{
    return _boneShieldStacks == 0 && HasEnoughResource(BONE_SHIELD);
}

bool BloodSpecialization::ShouldCastDancingRuneWeapon()
{
    return _dancingRuneWeaponReady == 0 && GetBot()->GetHealthPct() < 50.0f;
}

void BloodSpecialization::BuildThreat(::Unit* target)
{
    if (!target)
        return;

    // Generate threat through damage and abilities
    _totalThreatGenerated += 1000; // Approximate threat value
}

void BloodSpecialization::MaintainThreat()
{
    // Use AoE abilities when facing multiple targets
    if (_threatTargets.size() > 1)
    {
        if (ShouldCastBloodBoil())
            CastBloodBoil();
        else if (ShouldCastDeathAndDecay())
            CastDeathAndDecay(GetBot()->GetPosition());
    }
}

std::vector<::Unit*> BloodSpecialization::GetThreatTargets() const
{
    return _threatTargets;
}

bool BloodSpecialization::NeedsThreat(::Unit* target)
{
    if (!target)
        return false;

    // Check if target is attacking someone other than the tank
    return target->GetTarget() != GetBot()->GetGUID();
}

void BloodSpecialization::ManageSelfHealing()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (ShouldSelfHeal())
    {
        ::Unit* target = bot->GetSelectedUnit();
        if (target && ShouldCastDeathStrike(target))
        {
            CastDeathStrike(target);
        }
    }
}

bool BloodSpecialization::ShouldSelfHeal()
{
    return GetBot()->GetHealthPct() < SELF_HEAL_THRESHOLD * 100;
}

uint32 BloodSpecialization::CalculateHealingNeeded()
{
    Player* bot = GetBot();
    if (!bot)
        return 0;

    return bot->GetMaxHealth() - bot->GetHealth();
}

void BloodSpecialization::CastDeathStrike(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(DEATH_STRIKE))
    {
        bot->CastSpell(target, DEATH_STRIKE, false);
        ConsumeResource(DEATH_STRIKE);
        BuildThreat(target);

        // Death Strike heals based on recent damage taken
        uint32 healing = CalculateHealingNeeded() / 4; // 25% of missing health
        bot->SetHealth(std::min(bot->GetHealth() + healing, bot->GetMaxHealth()));
        _totalSelfHealing += healing;
    }
}

void BloodSpecialization::CastHeartStrike(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(HEART_STRIKE))
    {
        bot->CastSpell(target, HEART_STRIKE, false);
        ConsumeResource(HEART_STRIKE);
        BuildThreat(target);
    }
}

void BloodSpecialization::CastBloodBoil()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(BLOOD_BOIL))
    {
        bot->CastSpell(bot, BLOOD_BOIL, false);
        ConsumeResource(BLOOD_BOIL);

        // Generate threat on all nearby enemies
        for (::Unit* target : _threatTargets)
            BuildThreat(target);
    }
}

void BloodSpecialization::CastPlagueStrike(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(PLAGUE_STRIKE))
    {
        bot->CastSpell(target, PLAGUE_STRIKE, false);
        ConsumeResource(PLAGUE_STRIKE);
        ApplyDisease(target, DiseaseType::BLOOD_PLAGUE, PLAGUE_STRIKE);
        BuildThreat(target);
    }
}

void BloodSpecialization::CastDarkCommand(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (bot->HasSpell(DARK_COMMAND) && NeedsThreat(target))
    {
        bot->CastSpell(target, DARK_COMMAND, false);
        BuildThreat(target);
    }
}

void BloodSpecialization::CastDeathPact()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->HasSpell(DEATH_PACT) && bot->GetHealthPct() < 30.0f)
    {
        bot->CastSpell(bot, DEATH_PACT, false);
        uint32 healing = bot->GetMaxHealth() / 2; // 50% of max health
        bot->SetHealth(std::min(bot->GetHealth() + healing, bot->GetMaxHealth()));
        _totalSelfHealing += healing;
    }
}

void BloodSpecialization::CastVampiricBlood()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(VAMPIRIC_BLOOD))
    {
        bot->CastSpell(bot, VAMPIRIC_BLOOD, false);
        ConsumeResource(VAMPIRIC_BLOOD);
    }
}

void BloodSpecialization::CastBoneShield()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(BONE_SHIELD))
    {
        bot->CastSpell(bot, BONE_SHIELD, false);
        ConsumeResource(BONE_SHIELD);
    }
}

void BloodSpecialization::CastDancingRuneWeapon()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(DANCING_RUNE_WEAPON))
    {
        bot->CastSpell(bot, DANCING_RUNE_WEAPON, false);
        ConsumeResource(DANCING_RUNE_WEAPON);
    }
}

void BloodSpecialization::CastIceboundFortitude()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(ICEBOUND_FORTITUDE))
    {
        bot->CastSpell(bot, ICEBOUND_FORTITUDE, false);
        ConsumeResource(ICEBOUND_FORTITUDE);
    }
}

void BloodSpecialization::CastAntiMagicShell()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(ANTI_MAGIC_SHELL))
    {
        bot->CastSpell(bot, ANTI_MAGIC_SHELL, false);
        ConsumeResource(ANTI_MAGIC_SHELL);
    }
}

void BloodSpecialization::UseDefensiveCooldowns()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->GetHealthPct() < 30.0f && ShouldCastVampiricBlood())
        CastVampiricBlood();

    if (bot->GetHealthPct() < 40.0f && ShouldCastDancingRuneWeapon())
        CastDancingRuneWeapon();

    if (bot->GetHealthPct() < 25.0f && _iceboundFortitudeReady == 0)
        CastIceboundFortitude();
}

void BloodSpecialization::ManageEmergencyAbilities()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Priority: Death Pact for immediate healing
    if (bot->GetHealthPct() < 20.0f)
    {
        CastDeathPact();
        return;
    }

    // Secondary: Icebound Fortitude for damage reduction
    if (bot->GetHealthPct() < 25.0f && _iceboundFortitudeReady == 0)
    {
        CastIceboundFortitude();
        return;
    }

    // Tertiary: Vampiric Blood for increased healing
    if (ShouldCastVampiricBlood())
    {
        CastVampiricBlood();
        return;
    }
}

void BloodSpecialization::EnterBloodPresence()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->HasSpell(BLOOD_PRESENCE) && !bot->HasAura(BLOOD_PRESENCE))
    {
        bot->CastSpell(bot, BLOOD_PRESENCE, false);
    }
}

bool BloodSpecialization::ShouldUseBloodPresence()
{
    Player* bot = GetBot();
    return bot && bot->HasSpell(BLOOD_PRESENCE) && !bot->HasAura(BLOOD_PRESENCE);
}

} // namespace Playerbot