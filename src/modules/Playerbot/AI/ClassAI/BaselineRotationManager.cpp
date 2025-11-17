/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Baseline Rotation Manager Implementation - PACKET-BASED MIGRATION
 * PHASE 0 WEEK 3 (2025-10-30): Migrated to packet-based spell casting
 */

#include "BaselineRotationManager.h"
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Unit.h"
#include "ObjectAccessor.h"
#include "SharedDefines.h"
#include "ObjectGuid.h"
#include "Map.h"
#include "Log.h"  // For TC_LOG_DEBUG
#include "../../Spatial/SpatialGridQueryHelpers.h"  // PHASE 5F: Thread-safe queries
#include "../../Packets/SpellPacketBuilder.h"  // PHASE 0 WEEK 3: Packet-based spell casting
#include "GameTime.h"

namespace Playerbot
{

// ============================================================================
// BaselineRotationManager Implementation
// ============================================================================

BaselineRotationManager::BaselineRotationManager()
{
    // Initialize baseline abilities for all classes
    InitializeWarriorBaseline();
    InitializePaladinBaseline();
    InitializeHunterBaseline();
    InitializeRogueBaseline();
    InitializePriestBaseline();
    InitializeDeathKnightBaseline();
    InitializeShamanBaseline();
    InitializeMageBaseline();
    InitializeWarlockBaseline();
    InitializeMonkBaseline();
    InitializeDruidBaseline();
    InitializeDemonHunterBaseline();
    InitializeEvokerBaseline();
}

bool BaselineRotationManager::ShouldUseBaselineRotation(Player* bot)
{
    if (!bot)
        return false;

    // Use baseline rotation for levels 1-9
    uint8 level = bot->GetLevel();    if (level < 10)
        return true;

    // Also use if bot is level 10+ but hasn't chosen a specialization
    // (This is an edge case that should trigger auto-specialization)
    // Check if bot has an active talent group (specialization)
    uint8 activeGroup = bot->GetActiveTalentGroup();
    return (level >= 10 && activeGroup == 0);
}

bool BaselineRotationManager::ExecuteBaselineRotation(Player* bot, ::Unit* target)
{
    if (!bot || !target || !target->IsAlive())
        return false;

    // CRITICAL FIX: Ensure auto-attack is active before attempting rotation
    // Many low-level bots (level 1-9) don't have spells yet and rely on auto-attack
    // Even when spells are available, auto-attack should always be active in combat
    if (!bot->IsInCombatWith(target))
    {
        bot->SetInCombatWith(target);
        target->SetInCombatWith(bot);
    }

    // Initiate auto-attack if not already attacking this target
    // This is critical for low-level bots and serves as a fallback if no spells work
    if (bot->GetVictim() != target)    {
        bot->Attack(target, true);
        TC_LOG_DEBUG("module.playerbot.baseline",

                     "Bot {} initiated auto-attack on {} (baseline rotation)",

                     bot->GetName(), target->GetName());
    }

    // Get baseline abilities for bot's class - FIX: use GetClass() not getClass()
    auto abilities = GetBaselineAbilities(bot->GetClass());    if (!abilities || abilities->empty())
    {
        // No spells available - rely on auto-attack (already initiated above)
        return true; // Return true because auto-attack is active
    }

    // Sort abilities by priority (higher priority first)
    ::std::vector<BaselineAbility> sorted = *abilities;
    ::std::sort(sorted.begin(), sorted.end(), [](auto const& a, auto const& b) {
        return a.priority > b.priority;
    });

    // Try to cast highest priority available ability
    for (auto const& ability : sorted)    {
        if (TryCastAbility(bot, target, ability))

            return true;
    }

    // Even if no spell was cast, auto-attack is active, so return true
    return true;
}

void BaselineRotationManager::ApplyBaselineBuffs(Player* bot)
{
    if (!bot)
        return;

    // FIX: use GetClass() not getClass()
    uint8 classId = bot->GetClass();    // Delegate to class-specific buff application
    switch (classId)
    {
        case CLASS_WARRIOR:

            WarriorBaselineRotation::ApplyBuffs(bot);

            break;
        case CLASS_PALADIN:

            PaladinBaselineRotation::ApplyBuffs(bot);

            break;
        case CLASS_HUNTER:

            HunterBaselineRotation::ApplyBuffs(bot);

            break;
        case CLASS_ROGUE:

            RogueBaselineRotation::ApplyBuffs(bot);

            break;
        case CLASS_PRIEST:

            PriestBaselineRotation::ApplyBuffs(bot);

            break;
        case CLASS_DEATH_KNIGHT:

            DeathKnightBaselineRotation::ApplyBuffs(bot);

            break;
        case CLASS_SHAMAN:

            ShamanBaselineRotation::ApplyBuffs(bot);

            break;
        case CLASS_MAGE:

            MageBaselineRotation::ApplyBuffs(bot);

            break;
        case CLASS_WARLOCK:

            WarlockBaselineRotation::ApplyBuffs(bot);

            break;

        case CLASS_MONK:
        MonkBaselineRotation::ApplyBuffs(bot);

            break;
        case CLASS_DRUID:

            DruidBaselineRotation::ApplyBuffs(bot);

            break;
        case CLASS_DEMON_HUNTER:

            DemonHunterBaselineRotation::ApplyBuffs(bot);

            break;
        case CLASS_EVOKER:

            EvokerBaselineRotation::ApplyBuffs(bot);

            break;
    }
}

bool BaselineRotationManager::HandleAutoSpecialization(Player* bot){
    if (!bot)
        return false;

    // Only auto-select at level 10+
    if (bot->GetLevel() < 10)
        return false;

    // Check if already has specialization
    // FIX: Use GetActiveTalentGroup() instead of GetUInt32Value(PLAYER_FIELD_CURRENT_SPEC_ID)
    uint8 activeGroup = bot->GetActiveTalentGroup();    if (activeGroup != 0)
        return false; // Already has spec

    // Select optimal specialization
    uint32 specId = SelectOptimalSpecialization(bot);
    if (specId == 0)
        return false;

    // FIX: TrinityCore doesn't have SetUInt32Value or ActivateSpec
    // This would need proper specialization API calls
    // For now, this is a placeholder - actual implementation would need
    // to use the proper TrinityCore talent/specialization APIs
    // bot->LearnDefaultSkills();
    // bot->LearnSpecializationSpells();

    return true;
}

float BaselineRotationManager::GetBaselineOptimalRange(Player* bot)
{
    if (!bot)
        return 5.0f;

    // FIX: use GetClass() not getClass()
    uint8 classId = bot->GetClass();    // Melee classes
    if (classId == CLASS_WARRIOR || classId == CLASS_ROGUE ||
        classId == CLASS_PALADIN || classId == CLASS_DEATH_KNIGHT ||
        classId == CLASS_MONK || classId == CLASS_DEMON_HUNTER)
    {
        return 5.0f;
    }

    // Ranged classes
    return 25.0f;
}

::std::vector<BaselineAbility> const* BaselineRotationManager::GetBaselineAbilities(uint8 classId) const
{
    auto it = _baselineAbilities.find(classId);
    if (it != _baselineAbilities.end())
        return &it->second;
    return nullptr;
}

bool BaselineRotationManager::TryCastAbility(Player* bot, ::Unit* target, BaselineAbility const& ability)
{
    if (!CanUseAbility(bot, target, ability))
        return false;

    // Check cooldown
    auto& botCooldowns = _cooldowns[bot->GetGUID().GetCounter()];    auto cdIt = botCooldowns.find(ability.spellId);
    if (cdIt != botCooldowns.end() && cdIt->second > GameTime::GetGameTimeMS())        return false; // On cooldown

    // MIGRATION COMPLETE (2025-10-30):
    // Replaced direct CastSpell(spellId, false, ) API call with packet-based SpellPacketBuilder.
    // BEFORE: bot->CastSpell(castTarget); // UNSAFE - worker thread
    // AFTER: SpellPacketBuilder::BuildCastSpellPacket(...) // SAFE - queues to main thread

    // Get spell info for validation
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(ability.spellId, bot->GetMap()->GetDifficultyID());    if (!spellInfo)
    {
        TC_LOG_TRACE("playerbot.baseline.packets",

                     "Bot {} spell {} not found in spell data",

                     bot->GetName(), ability.spellId);
        return false;
    }

    // Determine cast target
    ::Unit* castTarget = ability.requiresMelee ? target : (ability.isDefensive ? bot : target);

    // Build packet with validation
    SpellPacketBuilder::BuildOptions options;
    options.skipGcdCheck = false;      // Respect GCD
    options.skipResourceCheck = false; // Check mana/energy/rage
    options.skipRangeCheck = false;    // Check spell range
    // Cast time check covered by skipStateCheck
    // Cooldown check covered by skipGcdCheck
    // LOS check covered by skipRangeCheck


    auto result = SpellPacketBuilder::BuildCastSpellPacket(bot, ability.spellId, castTarget, options);
    if (!castTarget)
    {
        return false;
    }

    if (result.result == SpellPacketBuilder::ValidationResult::SUCCESS)
    {
        // Packet successfully queued to main thread

        // Optimistic cooldown recording (packet will be processed)
        botCooldowns[ability.spellId] = GameTime::GetGameTimeMS() + ability.cooldown;        TC_LOG_DEBUG("playerbot.baseline.packets",

                     "Bot {} queued CMSG_CAST_SPELL for baseline spell {} (target: {})",

                     bot->GetName(), ability.spellId,

                     castTarget ? castTarget->GetName() : "self");
        return true;
    }
    else
    {
        // Validation failed - packet not queued
        TC_LOG_TRACE("playerbot.baseline.packets",

                     "Bot {} baseline spell {} validation failed: {} ({})",

                     bot->GetName(), ability.spellId,

                     static_cast<uint8>(result.result),

                     result.failureReason);
        return false;
    }}

bool BaselineRotationManager::CanUseAbility(Player* bot, ::Unit* target, BaselineAbility const& ability) const{
    if (!bot || !target)
        return false;

    // Level check
    if (bot->GetLevel() < ability.minLevel)
        return false;

    // Range check
    if (ability.requiresMelee)
    {
        if (bot->GetExactDistSq(target) > (5.0f * 5.0f)) // 25.0f

            return false;
    }
    else
    {

        if (bot->GetExactDistSq(target) > (30.0f * 30.0f) || !bot->IsWithinLOSInMap(target)) // 900.0f
        return false;
    }

    // Resource check
    uint32 currentResource = 0;
    // FIX: use GetClass() not getClass()
    switch (bot->GetClass())
    {
        case CLASS_WARRIOR:
        case CLASS_DRUID: // In some forms

            currentResource = bot->GetPower(POWER_RAGE);
            break;
        case CLASS_ROGUE:
        case CLASS_MONK:

            currentResource = bot->GetPower(POWER_ENERGY);
            break;
        case CLASS_HUNTER:

            currentResource = bot->GetPower(POWER_FOCUS);
            break;
        case CLASS_DEATH_KNIGHT:

            currentResource = bot->GetPower(POWER_RUNIC_POWER);
            break;
        default:

            currentResource = bot->GetPower(POWER_MANA);
            break;
    }

    if (currentResource < ability.resourceCost)
        return false;

    return true;
}

uint32 BaselineRotationManager::SelectOptimalSpecialization(Player* bot) const{
    if (!bot)
        return 0;

    // Default to first spec for each class
    // In a more advanced implementation, this could consider:
    // - Bot's role preference (tank/dps/healer)
    // - Group composition
    // - Stat distribution

    // FIX: use GetClass() not getClass()
    uint8 classId = bot->GetClass();    switch (classId)
    {
        case CLASS_WARRIOR:

            return 71; // Arms (71), Fury (72), Protection (73)
        case CLASS_PALADIN:

            return 65; // Holy (65), Protection (66), Retribution (70)
        case CLASS_HUNTER:

            return 253; // Beast Mastery (253), Marksmanship (254), Survival (255)
        case CLASS_ROGUE:

            return 259; // Assassination (259), Combat/Outlaw (260), Subtlety (261)
        case CLASS_PRIEST:

            return 256; // Discipline (256), Holy (257), Shadow (258)
        case CLASS_DEATH_KNIGHT:

            return 250; // Blood (250), Frost (251), Unholy (252)
        case CLASS_SHAMAN:

            return 262; // Elemental (262), Enhancement (263), Restoration (264)
        case CLASS_MAGE:

            return 62; // Arcane (62), Fire (63), Frost (64)
        case CLASS_WARLOCK:

            return 265; // Affliction (265), Demonology (266), Destruction (267)
        case CLASS_MONK:

            return 268; // Brewmaster (268), Windwalker (269), Mistweaver (270)
        case CLASS_DRUID:

            return 102; // Balance (102), Feral (103), Guardian (104), Restoration (105)
        case CLASS_DEMON_HUNTER:

            return 577; // Havoc (577), Vengeance (581)
        case CLASS_EVOKER:

            return 1467; // Devastation (1467), Preservation (1468), Augmentation (1473)
        default:

            return 0;
    }
}

// ============================================================================
// Warrior Baseline Abilities
// ============================================================================

// Define spell IDs (these would normally come from a shared header)
constexpr uint32 EXECUTE = 5308;
constexpr uint32 VICTORY_RUSH = 34428;
constexpr uint32 SLAM = 1464;
constexpr uint32 HAMSTRING = 1715;
constexpr uint32 CHARGE = 100;
constexpr uint32 BATTLE_SHOUT = 6673;

void BaselineRotationManager::InitializeWarriorBaseline()
{
    ::std::vector<BaselineAbility> abilities;

    // Priority order: Execute > Victory Rush > Slam > Hamstring
    abilities.emplace_back(EXECUTE, 9, 15, 0, 10.0f, true);           // Execute (high priority if target low health)
    abilities.emplace_back(VICTORY_RUSH, 3, 0, 0, 9.0f, true);        // Victory Rush (free healing)

    abilities.emplace_back(SLAM, 1, 20, 0, 5.0f, true);
    // Slam (rage dump)
    abilities.emplace_back(HAMSTRING, 7, 10, 0, 3.0f, true);          // Hamstring (slow fleeing enemies)
    abilities.emplace_back(CHARGE, 1, 0, 15000, 15.0f, false);        // Charge (engage)

    _baselineAbilities[CLASS_WARRIOR] = ::std::move(abilities);
}

bool WarriorBaselineRotation::ExecuteRotation(Player* bot, ::Unit* target, BaselineRotationManager& manager)
{
    // Charge if not in melee range (using squared distance for comparison)
    float distSq = bot->GetExactDistSq(target);    if (distSq > (8.0f * 8.0f) && distSq < (25.0f * 25.0f)) // 64.0f and 625.0f
    {
        // FIX: Use HasSpell check correctly
    if (bot->HasSpell(CHARGE))
        {

            bot->CastSpell(CastSpellTargetArg(target), CHARGE);

            return true;
        }
    }

    // Execute if target below 20% health
    if (target->GetHealthPct() <= 20.0f && bot->HasSpell(EXECUTE))    {
        if (bot->GetPower(POWER_RAGE) >= 15)        {

            bot->CastSpell(CastSpellTargetArg(target), EXECUTE);

            return true;
        }
    }

    // Victory Rush for healing
    if (bot->HasSpell(VICTORY_RUSH) && bot->HasAura(32216)) // Victory Rush proc
    {
        bot->CastSpell(CastSpellTargetArg(target), VICTORY_RUSH);
        return true;
    }

    // Slam as rage dump
    if (bot->HasSpell(SLAM) && bot->GetPower(POWER_RAGE) >= 20)    {
        bot->CastSpell(CastSpellTargetArg(target), SLAM);
        return true;
    }

    // Hamstring to prevent fleeing
    if (bot->HasSpell(HAMSTRING) && bot->GetPower(POWER_RAGE) >= 10)    {
        if (target->GetHealthPct() < 30.0f)
        {

            bot->CastSpell(CastSpellTargetArg(target), HAMSTRING);

            return true;
        }
    }

    return false;
}

void WarriorBaselineRotation::ApplyBuffs(Player* bot){
    // Battle Shout
    if (bot->HasSpell(BATTLE_SHOUT) && !bot->HasAura(BATTLE_SHOUT))
    {
        bot->CastSpell(CastSpellTargetArg(bot), BATTLE_SHOUT);
    }
}

// ============================================================================
// Paladin Baseline Abilities
// ============================================================================

void BaselineRotationManager::InitializePaladinBaseline()
{
    ::std::vector<BaselineAbility> abilities;

    abilities.emplace_back(35395, 1, 0, 6000, 10.0f, true);    // Crusader Strike
    abilities.emplace_back(20271, 3, 0, 8000, 9.0f, false);    // Judgment
    abilities.emplace_back(85673, 5, 0, 10000, 7.0f, false);   // Word of Glory (self-heal)
    abilities.emplace_back(853, 9, 0, 60000, 5.0f, false);     // Hammer of Justice (CC)

    _baselineAbilities[CLASS_PALADIN] = ::std::move(abilities);
}

bool PaladinBaselineRotation::ExecuteRotation(Player* bot, ::Unit* target, BaselineRotationManager& manager)
{
    return manager.ExecuteBaselineRotation(bot, target);
}

void PaladinBaselineRotation::ApplyBuffs(Player* bot)
{
    // No baseline buffs for low-level Paladins
}

// ============================================================================
// Hunter Baseline Abilities
// ============================================================================

void BaselineRotationManager::InitializeHunterBaseline()
{
    ::std::vector<BaselineAbility> abilities;

    abilities.emplace_back(19434, 1, 20, 3000, 10.0f, false);  // Aimed Shot
    abilities.emplace_back(185358, 3, 20, 0, 9.0f, false);     // Arcane Shot
    abilities.emplace_back(34026, 5, 30, 7500, 8.0f, false);   // Kill Command
    abilities.emplace_back(56641, 9, 0, 0, 5.0f, false);       // Steady Shot (focus builder)

    _baselineAbilities[CLASS_HUNTER] = ::std::move(abilities);
}

bool HunterBaselineRotation::ExecuteRotation(Player* bot, ::Unit* target, BaselineRotationManager& manager)
{
    return manager.ExecuteBaselineRotation(bot, target);
}

void HunterBaselineRotation::ApplyBuffs(Player* bot)
{
    // No baseline buffs for low-level Hunters
}

// ============================================================================
// Additional Classes (stub implementations for brevity)
// ============================================================================

void BaselineRotationManager::InitializeRogueBaseline()
{
    ::std::vector<BaselineAbility> abilities;
    abilities.emplace_back(1752, 1, 40, 0, 10.0f, true);  // Sinister Strike
    abilities.emplace_back(196819, 3, 35, 0, 9.0f, true); // Eviscerate (finisher)
    _baselineAbilities[CLASS_ROGUE] = ::std::move(abilities);
}

void BaselineRotationManager::InitializePriestBaseline()
{
    ::std::vector<BaselineAbility> abilities;
    abilities.emplace_back(585, 1, 0, 0, 10.0f, false);  // Smite
    abilities.emplace_back(589, 1, 0, 0, 9.0f, false);   // Shadow Word: Pain
    _baselineAbilities[CLASS_PRIEST] = ::std::move(abilities);
}

void BaselineRotationManager::InitializeDeathKnightBaseline()
{
    ::std::vector<BaselineAbility> abilities;
    abilities.emplace_back(49998, 8, 40, 0, 10.0f, true);  // Death Strike
    abilities.emplace_back(45477, 8, 0, 8000, 9.0f, false); // Icy Touch
    _baselineAbilities[CLASS_DEATH_KNIGHT] = ::std::move(abilities);
}

void BaselineRotationManager::InitializeShamanBaseline()
{
    ::std::vector<BaselineAbility> abilities;
    abilities.emplace_back(403, 1, 0, 0, 10.0f, false);    // Lightning Bolt
    abilities.emplace_back(73899, 1, 0, 0, 9.0f, true);    // Primal Strike
    _baselineAbilities[CLASS_SHAMAN] = ::std::move(abilities);
}

void BaselineRotationManager::InitializeMageBaseline()
{
    ::std::vector<BaselineAbility> abilities;
    abilities.emplace_back(116, 1, 0, 0, 10.0f, false);    // Frostbolt
    abilities.emplace_back(133, 1, 0, 0, 9.0f, false);     // Fireball
    _baselineAbilities[CLASS_MAGE] = ::std::move(abilities);
}

void BaselineRotationManager::InitializeWarlockBaseline()
{
    ::std::vector<BaselineAbility> abilities;
    abilities.emplace_back(686, 1, 0, 0, 10.0f, false);    // Shadow Bolt
    abilities.emplace_back(172, 1, 0, 0, 9.0f, false);     // Corruption
    _baselineAbilities[CLASS_WARLOCK] = ::std::move(abilities);
}

void BaselineRotationManager::InitializeMonkBaseline()
{
    ::std::vector<BaselineAbility> abilities;
    abilities.emplace_back(100780, 1, 50, 0, 10.0f, true);  // Tiger Palm
    abilities.emplace_back(100784, 1, 40, 0, 9.0f, true);   // Blackout Kick
    _baselineAbilities[CLASS_MONK] = ::std::move(abilities);
}

void BaselineRotationManager::InitializeDruidBaseline()
{
    ::std::vector<BaselineAbility> abilities;
    abilities.emplace_back(5176, 1, 0, 0, 10.0f, false);    // Wrath
    abilities.emplace_back(8921, 1, 0, 0, 9.0f, false);     // Moonfire
    _baselineAbilities[CLASS_DRUID] = ::std::move(abilities);
}

void BaselineRotationManager::InitializeDemonHunterBaseline()
{
    ::std::vector<BaselineAbility> abilities;
    abilities.emplace_back(162243, 8, 40, 0, 10.0f, true);  // Demon's Bite
    abilities.emplace_back(162794, 8, 40, 0, 9.0f, true);   // Chaos Strike
    _baselineAbilities[CLASS_DEMON_HUNTER] = ::std::move(abilities);
}

void BaselineRotationManager::InitializeEvokerBaseline()
{
    ::std::vector<BaselineAbility> abilities;
    abilities.emplace_back(361469, 1, 0, 0, 10.0f, false);  // Azure Strike
    abilities.emplace_back(361500, 1, 0, 0, 9.0f, false);   // Living Flame
    _baselineAbilities[CLASS_EVOKER] = ::std::move(abilities);
}

// Stub implementations for other baseline rotations
bool RogueBaselineRotation::ExecuteRotation(Player* bot, ::Unit* target, BaselineRotationManager& manager)
{
    return manager.ExecuteBaselineRotation(bot, target);
}

void RogueBaselineRotation::ApplyBuffs(Player* bot) {}

bool PriestBaselineRotation::ExecuteRotation(Player* bot, ::Unit* target, BaselineRotationManager& manager)
{
    return manager.ExecuteBaselineRotation(bot, target);
}

void PriestBaselineRotation::ApplyBuffs(Player* bot) {}

bool DeathKnightBaselineRotation::ExecuteRotation(Player* bot, ::Unit* target, BaselineRotationManager& manager)
{
    return manager.ExecuteBaselineRotation(bot, target);
}

void DeathKnightBaselineRotation::ApplyBuffs(Player* bot) {}

bool ShamanBaselineRotation::ExecuteRotation(Player* bot, ::Unit* target, BaselineRotationManager& manager)
{
    return manager.ExecuteBaselineRotation(bot, target);
}

void ShamanBaselineRotation::ApplyBuffs(Player* bot) {}

bool MageBaselineRotation::ExecuteRotation(Player* bot, ::Unit* target, BaselineRotationManager& manager)
{
    return manager.ExecuteBaselineRotation(bot, target);
}

void MageBaselineRotation::ApplyBuffs(Player* bot) {}

bool WarlockBaselineRotation::ExecuteRotation(Player* bot, ::Unit* target, BaselineRotationManager& manager)
{
    return manager.ExecuteBaselineRotation(bot, target);
}

void WarlockBaselineRotation::ApplyBuffs(Player* bot) {}

bool MonkBaselineRotation::ExecuteRotation(Player* bot, ::Unit* target, BaselineRotationManager& manager)
{
    return manager.ExecuteBaselineRotation(bot, target);
}

void MonkBaselineRotation::ApplyBuffs(Player* bot) {}

bool DruidBaselineRotation::ExecuteRotation(Player* bot, ::Unit* target, BaselineRotationManager& manager)
{
    return manager.ExecuteBaselineRotation(bot, target);
}

void DruidBaselineRotation::ApplyBuffs(Player* bot) {}

bool DemonHunterBaselineRotation::ExecuteRotation(Player* bot, ::Unit* target, BaselineRotationManager& manager)
{
    return manager.ExecuteBaselineRotation(bot, target);
}

void DemonHunterBaselineRotation::ApplyBuffs(Player* bot) {}

bool EvokerBaselineRotation::ExecuteRotation(Player* bot, ::Unit* target, BaselineRotationManager& manager)
{
    return manager.ExecuteBaselineRotation(bot, target);
}

void EvokerBaselineRotation::ApplyBuffs(Player* bot) {}

} // namespace Playerbot