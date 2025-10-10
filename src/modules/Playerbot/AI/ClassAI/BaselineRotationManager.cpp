/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Baseline Rotation Manager Implementation - ENTERPRISE-GRADE VERSION
 * Uses proper spell queueing system mirroring TrinityCore player behavior
 */

#include "BaselineRotationManager.h"
#include "ClassAI.h"  // For RequestBotSpellCast() - enterprise spell queueing
#include "../Session/BotSession.h"  // For BotSession::GetAI() - proper AI accessor
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellDefines.h"
#include "Unit.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "SharedDefines.h"
#include "ObjectGuid.h"
#include "Map.h"
#include "Log.h"
#include "GameTime.h"
#include "Group.h"

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
    uint8 level = bot->GetLevel();
    if (level < 10)
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
    {
        TC_LOG_ERROR("module.playerbot.baseline", "❌ ExecuteBaselineRotation: Invalid input - bot={}, target={}, target_alive={}",
                     (void*)bot, (void*)target, target ? target->IsAlive() : false);
        return false;
    }

    uint8 classId = bot->GetClass();
    TC_LOG_ERROR("module.playerbot.baseline", "🎯 ExecuteBaselineRotation: Bot {} (class {}) executing baseline rotation",
                 bot->GetName(), classId);

    // Get baseline abilities for bot's class - FIX: use GetClass() not getClass()
    auto abilities = GetBaselineAbilities(classId);
    if (!abilities)
    {
        TC_LOG_ERROR("module.playerbot.baseline", "❌ ExecuteBaselineRotation: NO ABILITIES found for class {} - abilities pointer is NULL!",
                     classId);
        return false;
    }

    if (abilities->empty())
    {
        TC_LOG_ERROR("module.playerbot.baseline", "❌ ExecuteBaselineRotation: Abilities vector EMPTY for class {}!",
                     classId);
        return false;
    }

    TC_LOG_ERROR("module.playerbot.baseline", "✅ ExecuteBaselineRotation: Found {} abilities for class {}",
                 abilities->size(), classId);

    // Sort abilities by priority (higher priority first)
    std::vector<BaselineAbility> sorted = *abilities;
    std::sort(sorted.begin(), sorted.end(), [](auto const& a, auto const& b) {
        return a.priority > b.priority;
    });

    TC_LOG_ERROR("module.playerbot.baseline", "🔄 ExecuteBaselineRotation: Trying {} abilities in priority order",
                 sorted.size());

    // Try to cast highest priority available ability
    for (auto const& ability : sorted)
    {
        TC_LOG_ERROR("module.playerbot.baseline", "  → Trying ability spellId={} priority={:.1f}",
                     ability.spellId, ability.priority);

        if (TryCastAbility(bot, target, ability))
        {
            TC_LOG_ERROR("module.playerbot.baseline", "✅ ExecuteBaselineRotation: Successfully cast spell {}",
                         ability.spellId);
            return true;
        }
    }

    TC_LOG_ERROR("module.playerbot.baseline", "❌ ExecuteBaselineRotation: NO abilities could be cast!");
    return false;
}

void BaselineRotationManager::ApplyBaselineBuffs(Player* bot)
{
    if (!bot)
        return;

    // FIX: use GetClass() not getClass()
    uint8 classId = bot->GetClass();

    // Delegate to class-specific buff application
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

bool BaselineRotationManager::HandleAutoSpecialization(Player* bot)
{
    if (!bot)
        return false;

    // Only auto-select at level 10+
    if (bot->GetLevel() < 10)
        return false;

    // Check if already has specialization
    // FIX: Use GetActiveTalentGroup() instead of GetUInt32Value(PLAYER_FIELD_CURRENT_SPEC_ID)
    uint8 activeGroup = bot->GetActiveTalentGroup();
    if (activeGroup != 0)
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
    uint8 classId = bot->GetClass();

    // Melee classes
    if (classId == CLASS_WARRIOR || classId == CLASS_ROGUE ||
        classId == CLASS_PALADIN || classId == CLASS_DEATH_KNIGHT ||
        classId == CLASS_MONK || classId == CLASS_DEMON_HUNTER)
    {
        return 5.0f;
    }

    // Ranged classes
    return 25.0f;
}

std::vector<BaselineAbility> const* BaselineRotationManager::GetBaselineAbilities(uint8 classId) const
{
    auto it = _baselineAbilities.find(classId);
    if (it != _baselineAbilities.end())
        return &it->second;
    return nullptr;
}

bool BaselineRotationManager::TryCastAbility(Player* bot, ::Unit* target, BaselineAbility const& ability)
{
    if (!CanUseAbility(bot, target, ability))
    {
        TC_LOG_ERROR("module.playerbot.baseline", "❌ Bot {} cannot use spell {} - failed CanUseAbility check",
                     bot->GetName(), ability.spellId);
        return false;
    }

    // Check cooldown
    auto& botCooldowns = _cooldowns[bot->GetGUID().GetCounter()];
    auto cdIt = botCooldowns.find(ability.spellId);
    if (cdIt != botCooldowns.end() && cdIt->second > getMSTime())
    {
        TC_LOG_TRACE("module.playerbot.baseline", "Spell {} on cooldown for {} ms",
                     ability.spellId, cdIt->second - getMSTime());
        return false; // On cooldown
    }

    // Cast spell
    // FIX: GetSpellInfo now requires difficulty parameter
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(ability.spellId, bot->GetMap()->GetDifficultyID());
    if (!spellInfo)
    {
        TC_LOG_ERROR("module.playerbot.baseline", "❌ Bot {} - spell {} not found in SpellMgr",
                     bot->GetName(), ability.spellId);
        return false;
    }

    // Check if bot knows the spell
    if (!bot->HasSpell(ability.spellId))
    {
        TC_LOG_ERROR("module.playerbot.baseline", "❌ Bot {} (level {}) does not know spell {}",
                     bot->GetName(), bot->GetLevel(), ability.spellId);
        return false;
    }

    // Determine cast target
    ::Unit* castTarget = ability.requiresMelee ? target : (ability.isDefensive ? bot : target);

    // CRITICAL: Validate target before casting
    if (!castTarget || !castTarget->IsInWorld() || !castTarget->IsAlive())
    {
        TC_LOG_ERROR("module.playerbot.baseline", "❌ Bot {} - Invalid cast target (dead or not in world)",
                     bot->GetName());
        return false;
    }

    // For offensive spells, validate target is attackable
    if (!ability.isDefensive)
    {
        // Check basic attack validity - can we attack this type of unit?
        if (castTarget->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE) ||
            castTarget->HasUnitFlag(UNIT_FLAG_UNINTERACTIBLE) ||
            castTarget->IsFriendlyTo(bot))
        {
            TC_LOG_ERROR("module.playerbot.baseline", "❌ Bot {} - Target {} is friendly or non-attackable",
                         bot->GetName(), castTarget->GetName());
            return false;
        }

        // CRITICAL FIX: Allow first offensive spell to initiate combat
        // Instead of requiring bot->IsHostileTo() before casting,
        // we allow the spell cast itself to establish hostility.
        // This is essential for attacking neutral mobs.
        //
        // The spell cast will fail naturally if the target is truly invalid,
        // so we don't need strict pre-validation here.
        //
        // NOTE: We still check for victim to ensure combat has been initiated
        // by bot->Attack() in GroupCombatStrategy, but we don't require
        // the hostility relationship to be fully established yet.
        if (!bot->GetVictim() || bot->GetVictim() != castTarget)
        {
            TC_LOG_ERROR("module.playerbot.baseline", "⚠️ Bot {} - Target {} not set as victim yet (combat initiation in progress)",
                         bot->GetName(), castTarget->GetName());
            // Allow the cast to proceed - it will establish the hostility
        }
    }

    TC_LOG_ERROR("module.playerbot.baseline", "🎯 Bot {} attempting to cast spell {} on {}",
                 bot->GetName(), ability.spellId, castTarget->GetName());

    // CRITICAL FIX: Stop movement before casting spells with cast time
    // Ranged spells require the caster to be stationary
    if (!ability.requiresMelee && bot->isMoving())
    {
        bot->StopMoving();
        bot->GetMotionMaster()->Clear();
        TC_LOG_ERROR("module.playerbot.baseline", "🛑 Stopped bot movement for spell casting");
    }

    // CRITICAL FIX: Ensure bot is facing the target before casting
    if (!bot->HasInArc(static_cast<float>(M_PI), castTarget))
    {
        bot->SetFacingToObject(castTarget);
        TC_LOG_ERROR("module.playerbot.baseline", "🎯 Adjusted bot facing towards target");
    }

    // ============================================================================
    // ENTERPRISE-GRADE FIX: Proper Spell Queueing System
    // ============================================================================
    // Uses ClassAI::RequestBotSpellCast() which mirrors TrinityCore's player
    // spell queueing architecture. This provides proper validation timing:
    //
    // 1. Spell is QUEUED (can queue within 400ms of GCD/cast completion)
    // 2. Queue is processed every frame in ClassAI::OnCombatUpdate()
    // 3. When ready, ExecutePendingSpell() creates Spell object with TRIGGERED_NONE
    // 4. Spell::prepare() handles validation at EXECUTE time (not queue time!)
    //
    // This matches retail WoW behavior:
    // - Player casts on neutral mob → spell queued → cast starts
    // - Spell executes → damage applied → mob becomes hostile
    // - Target validation happens when spell lands, not when queued
    //
    // NO MORE WORKAROUNDS - uses proper TrinityCore spell pipeline
    // See Player.cpp:30904-31051 and ClassAI.cpp:545-748 for implementation
    // ============================================================================

    // Get bot's AI through BotSession - proper architecture path
    // Player inherits from Unit which has GetAI() returning UnitAI*
    // But bots use BotAI/ClassAI stored in BotSession (separate hierarchy)
    BotSession* botSession = dynamic_cast<BotSession*>(bot->GetSession());
    if (!botSession)
    {
        TC_LOG_ERROR("module.playerbot.baseline", "❌ Bot {} session is not BotSession",
                    bot->GetName());
        return false;
    }

    BotAI* botAI = botSession->GetAI();
    if (!botAI)
    {
        TC_LOG_ERROR("module.playerbot.baseline", "❌ Bot {} has no BotAI",
                    bot->GetName());
        return false;
    }

    // Cast to ClassAI - safe because playerbots in combat use ClassAI
    ClassAI* classAI = dynamic_cast<ClassAI*>(botAI);
    if (!classAI)
    {
        TC_LOG_ERROR("module.playerbot.baseline", "❌ Bot {} AI is not ClassAI - cannot use spell queue system",
                    bot->GetName());
        return false;
    }

    // Request spell cast through proper queueing system
    bool queued = classAI->RequestBotSpellCast(ability.spellId, castTarget);
    if (queued)
    {
        // Record cooldown (will be enforced by spell system, but we track for priority logic)
        botCooldowns[ability.spellId] = getMSTime() + ability.cooldown;

        TC_LOG_DEBUG("module.playerbot.baseline", "✅ Bot {} queued spell {} on {} (enterprise-grade queueing)",
                     bot->GetName(), ability.spellId, castTarget->GetName());
        return true;
    }
    else
    {
        TC_LOG_TRACE("module.playerbot.baseline", "⚠️ Bot {} failed to queue spell {} - GCD/cast time > 400ms",
                     bot->GetName(), ability.spellId);
    }

    return false;
}

bool BaselineRotationManager::CanUseAbility(Player* bot, ::Unit* target, BaselineAbility const& ability) const
{
    if (!bot || !target)
    {
        TC_LOG_ERROR("module.playerbot.baseline", "❌ CanUseAbility: bot or target is NULL");
        return false;
    }

    // Level check
    if (bot->GetLevel() < ability.minLevel)
    {
        TC_LOG_ERROR("module.playerbot.baseline", "❌ Bot {} level {} < required level {} for spell {}",
                     bot->GetName(), bot->GetLevel(), ability.minLevel, ability.spellId);
        return false;
    }

    // Range check
    float distance = bot->GetDistance(target);
    if (ability.requiresMelee)
    {
        if (distance > 5.0f)
        {
            TC_LOG_ERROR("module.playerbot.baseline", "❌ Bot {} out of melee range ({:.1f}yd > 5.0yd) for spell {}",
                         bot->GetName(), distance, ability.spellId);
            return false;
        }
    }
    else
    {
        if (distance > 30.0f)
        {
            TC_LOG_ERROR("module.playerbot.baseline", "❌ Bot {} out of spell range ({:.1f}yd > 30.0yd) for spell {}",
                         bot->GetName(), distance, ability.spellId);
            return false;
        }

        if (!bot->IsWithinLOSInMap(target))
        {
            TC_LOG_ERROR("module.playerbot.baseline", "❌ Bot {} has no LOS to target for spell {}",
                         bot->GetName(), ability.spellId);
            return false;
        }
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
    {
        TC_LOG_ERROR("module.playerbot.baseline", "❌ Bot {} insufficient resources ({} < {}) for spell {}",
                     bot->GetName(), currentResource, ability.resourceCost, ability.spellId);
        return false;
    }

    TC_LOG_ERROR("module.playerbot.baseline", "✅ Bot {} CAN use spell {} (level OK, range {:.1f}yd OK, resources {} OK)",
                 bot->GetName(), ability.spellId, distance, currentResource);
    return true;
}

uint32 BaselineRotationManager::SelectOptimalSpecialization(Player* bot) const
{
    if (!bot)
        return 0;

    // Default to first spec for each class
    // In a more advanced implementation, this could consider:
    // - Bot's role preference (tank/dps/healer)
    // - Group composition
    // - Stat distribution

    // FIX: use GetClass() not getClass()
    uint8 classId = bot->GetClass();
    switch (classId)
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
    std::vector<BaselineAbility> abilities;

    // Priority order: Execute > Victory Rush > Slam > Hamstring
    abilities.emplace_back(EXECUTE, 9, 15, 0, 10.0f, true);           // Execute (high priority if target low health)
    abilities.emplace_back(VICTORY_RUSH, 3, 0, 0, 9.0f, true);        // Victory Rush (free healing)
    abilities.emplace_back(SLAM, 1, 20, 0, 5.0f, true);               // Slam (rage dump)
    abilities.emplace_back(HAMSTRING, 7, 10, 0, 3.0f, true);          // Hamstring (slow fleeing enemies)
    abilities.emplace_back(CHARGE, 1, 0, 15000, 15.0f, false);        // Charge (engage)

    _baselineAbilities[CLASS_WARRIOR] = std::move(abilities);
}

bool WarriorBaselineRotation::ExecuteRotation(Player* bot, ::Unit* target, BaselineRotationManager& manager)
{
    // Charge if not in melee range
    if (bot->GetDistance(target) > 8.0f && bot->GetDistance(target) < 25.0f)
    {
        // FIX: Use HasSpell check correctly
        if (bot->HasSpell(CHARGE))
        {
            bot->CastSpell(target, CHARGE, false);
            return true;
        }
    }

    // Execute if target below 20% health
    if (target->GetHealthPct() <= 20.0f && bot->HasSpell(EXECUTE))
    {
        if (bot->GetPower(POWER_RAGE) >= 15)
        {
            bot->CastSpell(target, EXECUTE, false);
            return true;
        }
    }

    // Victory Rush for healing
    if (bot->HasSpell(VICTORY_RUSH) && bot->HasAura(32216)) // Victory Rush proc
    {
        bot->CastSpell(target, VICTORY_RUSH, false);
        return true;
    }

    // Slam as rage dump
    if (bot->HasSpell(SLAM) && bot->GetPower(POWER_RAGE) >= 20)
    {
        bot->CastSpell(target, SLAM, false);
        return true;
    }

    // Hamstring to prevent fleeing
    if (bot->HasSpell(HAMSTRING) && bot->GetPower(POWER_RAGE) >= 10)
    {
        if (target->GetHealthPct() < 30.0f)
        {
            bot->CastSpell(target, HAMSTRING, false);
            return true;
        }
    }

    return false;
}

void WarriorBaselineRotation::ApplyBuffs(Player* bot)
{
    // Battle Shout
    if (bot->HasSpell(BATTLE_SHOUT) && !bot->HasAura(BATTLE_SHOUT))
    {
        bot->CastSpell(bot, BATTLE_SHOUT, false);
    }
}

// ============================================================================
// Paladin Baseline Abilities
// ============================================================================

void BaselineRotationManager::InitializePaladinBaseline()
{
    std::vector<BaselineAbility> abilities;

    abilities.emplace_back(35395, 1, 0, 6000, 10.0f, true);    // Crusader Strike
    abilities.emplace_back(20271, 3, 0, 8000, 9.0f, false);    // Judgment
    abilities.emplace_back(85673, 5, 0, 10000, 7.0f, false);   // Word of Glory (self-heal)
    abilities.emplace_back(853, 9, 0, 60000, 5.0f, false);     // Hammer of Justice (CC)

    _baselineAbilities[CLASS_PALADIN] = std::move(abilities);
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
    std::vector<BaselineAbility> abilities;

    abilities.emplace_back(19434, 1, 20, 3000, 10.0f, false);  // Aimed Shot
    abilities.emplace_back(185358, 3, 20, 0, 9.0f, false);     // Arcane Shot
    abilities.emplace_back(34026, 5, 30, 7500, 8.0f, false);   // Kill Command
    abilities.emplace_back(56641, 9, 0, 0, 5.0f, false);       // Steady Shot (focus builder)

    _baselineAbilities[CLASS_HUNTER] = std::move(abilities);
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
    std::vector<BaselineAbility> abilities;
    abilities.emplace_back(1752, 1, 40, 0, 10.0f, true);  // Sinister Strike
    abilities.emplace_back(196819, 3, 35, 0, 9.0f, true); // Eviscerate (finisher)
    _baselineAbilities[CLASS_ROGUE] = std::move(abilities);
}

void BaselineRotationManager::InitializePriestBaseline()
{
    std::vector<BaselineAbility> abilities;
    abilities.emplace_back(585, 1, 0, 0, 10.0f, false);  // Smite
    abilities.emplace_back(589, 1, 0, 0, 9.0f, false);   // Shadow Word: Pain
    _baselineAbilities[CLASS_PRIEST] = std::move(abilities);
}

void BaselineRotationManager::InitializeDeathKnightBaseline()
{
    std::vector<BaselineAbility> abilities;
    abilities.emplace_back(49998, 8, 40, 0, 10.0f, true);  // Death Strike
    abilities.emplace_back(45477, 8, 0, 8000, 9.0f, false); // Icy Touch
    _baselineAbilities[CLASS_DEATH_KNIGHT] = std::move(abilities);
}

void BaselineRotationManager::InitializeShamanBaseline()
{
    std::vector<BaselineAbility> abilities;
    abilities.emplace_back(403, 1, 0, 0, 10.0f, false);    // Lightning Bolt
    abilities.emplace_back(73899, 1, 0, 0, 9.0f, true);    // Primal Strike
    _baselineAbilities[CLASS_SHAMAN] = std::move(abilities);
}

void BaselineRotationManager::InitializeMageBaseline()
{
    std::vector<BaselineAbility> abilities;
    // Level 1: Fireball only
    // Level 4+: Frostbolt becomes available
    abilities.emplace_back(133, 1, 0, 0, 10.0f, false);     // Fireball (level 1+)
    abilities.emplace_back(116, 4, 0, 0, 9.0f, false);      // Frostbolt (level 4+)
    _baselineAbilities[CLASS_MAGE] = std::move(abilities);
}

void BaselineRotationManager::InitializeWarlockBaseline()
{
    std::vector<BaselineAbility> abilities;
    abilities.emplace_back(686, 1, 0, 0, 10.0f, false);    // Shadow Bolt
    abilities.emplace_back(172, 1, 0, 0, 9.0f, false);     // Corruption
    _baselineAbilities[CLASS_WARLOCK] = std::move(abilities);
}

void BaselineRotationManager::InitializeMonkBaseline()
{
    std::vector<BaselineAbility> abilities;
    abilities.emplace_back(100780, 1, 50, 0, 10.0f, true);  // Tiger Palm
    abilities.emplace_back(100784, 1, 40, 0, 9.0f, true);   // Blackout Kick
    _baselineAbilities[CLASS_MONK] = std::move(abilities);
}

void BaselineRotationManager::InitializeDruidBaseline()
{
    std::vector<BaselineAbility> abilities;
    abilities.emplace_back(5176, 1, 0, 0, 10.0f, false);    // Wrath
    abilities.emplace_back(8921, 1, 0, 0, 9.0f, false);     // Moonfire
    _baselineAbilities[CLASS_DRUID] = std::move(abilities);
}

void BaselineRotationManager::InitializeDemonHunterBaseline()
{
    std::vector<BaselineAbility> abilities;
    abilities.emplace_back(162243, 8, 40, 0, 10.0f, true);  // Demon's Bite
    abilities.emplace_back(162794, 8, 40, 0, 9.0f, true);   // Chaos Strike
    _baselineAbilities[CLASS_DEMON_HUNTER] = std::move(abilities);
}

void BaselineRotationManager::InitializeEvokerBaseline()
{
    std::vector<BaselineAbility> abilities;
    abilities.emplace_back(361469, 1, 0, 0, 10.0f, false);  // Azure Strike
    abilities.emplace_back(361500, 1, 0, 0, 9.0f, false);   // Living Flame
    _baselineAbilities[CLASS_EVOKER] = std::move(abilities);
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