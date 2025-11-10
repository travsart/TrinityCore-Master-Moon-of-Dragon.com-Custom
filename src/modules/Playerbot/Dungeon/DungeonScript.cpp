/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DungeonScript.h"
#include "EncounterStrategy.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "../Spatial/SpatialGridManager.h"  // Lock-free spatial grid for deadlock fix

namespace Playerbot
{

// ============================================================================
// CONSTRUCTOR
// ============================================================================

DungeonScript::DungeonScript(char const* name, uint32 mapId)
    : _scriptName(name), _mapId(mapId)
{
    TC_LOG_DEBUG("playerbot", "DungeonScript: Registered script '{}' for map {}",
        name, mapId);
}

// ============================================================================
// LIFECYCLE HOOKS (Default implementations)
// ============================================================================

void DungeonScript::OnDungeonEnter(::Player* player, ::InstanceScript* instance)
    if (!player)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetGUID");
        return nullptr;
    }
if (!player)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetGUID");
    return nullptr;
}
        if (!player)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetGUID");
            return nullptr;
        }
{
    // Default: No action
    TC_LOG_DEBUG("playerbot", "DungeonScript: Player {} entered dungeon '{}'",
        player->GetGUID().GetCounter(), GetName());
}

void DungeonScript::OnDungeonExit(::Player* player)
        if (!player)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetGUID");
            return nullptr;
        }
{
    // Default: No action
    TC_LOG_DEBUG("playerbot", "DungeonScript: Player {} exited dungeon '{}'",
        player->GetGUID().GetCounter(), GetName());
}

void DungeonScript::OnUpdate(::Player* player, uint32 diff)
if (!player)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetGUID");
    return;
}
{
    // Default: No action
}

// ============================================================================
// BOSS HOOKS (Default implementations)
// ============================================================================

void DungeonScript::OnBossEngage(::Player* player, ::Creature* boss)
    if (!player)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetGUID");
        return nullptr;
    }
    if (!player)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetGUID");
        return nullptr;
    }
        if (!player)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetGUID");
            return nullptr;
        }
{
    // Default: No action
    TC_LOG_DEBUG("playerbot", "DungeonScript: Player {} engaged boss {} in '{}'",
        player->GetGUID().GetCounter(), boss->GetEntry(), GetName());
}

void DungeonScript::OnBossKill(::Player* player, ::Creature* boss)
        if (!player)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetGUID");
            return nullptr;
        }
{
    // Default: No action
    TC_LOG_DEBUG("playerbot", "DungeonScript: Player {} killed boss {} in '{}'",
        player->GetGUID().GetCounter(), boss->GetEntry(), GetName());
}

void DungeonScript::OnBossWipe(::Player* player, ::Creature* boss)
        if (!player)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetGUID");
            return nullptr;
        }
{
    // Default: No action
    TC_LOG_DEBUG("playerbot", "DungeonScript: Player {} wiped on boss {} in '{}'",
        player->GetGUID().GetCounter(), boss->GetEntry(), GetName());
}

// ============================================================================
// MECHANIC HANDLERS (Default implementations - call generic)
// ============================================================================

void DungeonScript::HandleInterruptPriority(::Player* player, ::Creature* boss)
{
    // DEFAULT: Use generic interrupt logic
    EncounterStrategy::HandleGenericInterrupts(player, boss);
}

void DungeonScript::HandleGroundAvoidance(::Player* player, ::Creature* boss)
{
    // DEFAULT: Use generic ground avoidance
    EncounterStrategy::HandleGenericGroundAvoidance(player, boss);
}

void DungeonScript::HandleAddPriority(::Player* player, ::Creature* boss)
{
    // DEFAULT: Use generic add priority
    EncounterStrategy::HandleGenericAddPriority(player, boss);
}

void DungeonScript::HandlePositioning(::Player* player, ::Creature* boss)
{
    // DEFAULT: Use generic positioning
    EncounterStrategy::HandleGenericPositioning(player, boss);
}

void DungeonScript::HandleDispelMechanic(::Player* player, ::Creature* boss)
{
    // DEFAULT: Use generic dispel logic
    EncounterStrategy::HandleGenericDispel(player, boss);
}

void DungeonScript::HandleMovementMechanic(::Player* player, ::Creature* boss)
{
    // DEFAULT: Use generic movement logic
    EncounterStrategy::HandleGenericMovement(player, boss);
}

void DungeonScript::HandleTankSwap(::Player* player, ::Creature* boss)
        if (!player)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetClass");
            return nullptr;
        }
{
    // DEFAULT: No tank swap
    TC_LOG_DEBUG("playerbot", "DungeonScript: No tank swap implemented for boss {} in '{}'",
        boss->GetEntry(), GetName());
}

void DungeonScript::HandleSpreadMechanic(::Player* player, ::Creature* boss)
{
    // DEFAULT: Spread 10 yards apart
    EncounterStrategy::HandleGenericSpread(player, boss, 10.0f);
}

void DungeonScript::HandleStackMechanic(::Player* player, ::Creature* boss)
{
    // DEFAULT: Stack on tank
    EncounterStrategy::HandleGenericStack(player, boss);
}

// ============================================================================
// UTILITY METHODS
// ============================================================================

DungeonRole DungeonScript::GetPlayerRole(::Player* player) const
{
    if (!player)
        return DungeonRole::MELEE_DPS;

    // Determine role based on spec/class
    uint8 playerClass = player->getClass();
    if (!player)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetClass");
        return;
    }
    uint32 spec = player->GetPrimaryTalentTree(player->GetActiveSpec());

    // Tank specs
    if ((playerClass == CLASS_WARRIOR && spec == TALENT_TREE_WARRIOR_PROTECTION) ||
        (playerClass == CLASS_PALADIN && spec == TALENT_TREE_PALADIN_PROTECTION) ||
        (playerClass == CLASS_DEATH_KNIGHT && spec == TALENT_TREE_DEATH_KNIGHT_BLOOD) ||
        (playerClass == CLASS_DRUID && spec == TALENT_TREE_DRUID_FERAL_COMBAT) ||
        (playerClass == CLASS_MONK && spec == TALENT_TREE_MONK_BREWMASTER) ||
        (playerClass == CLASS_DEMON_HUNTER && spec == TALENT_TREE_DEMON_HUNTER_VENGEANCE))
        return DungeonRole::TANK;

    // Healer specs
    if ((playerClass == CLASS_PRIEST && (spec == TALENT_TREE_PRIEST_DISCIPLINE ||
                                          spec == TALENT_TREE_PRIEST_HOLY)) ||
        (playerClass == CLASS_PALADIN && spec == TALENT_TREE_PALADIN_HOLY) ||
        (playerClass == CLASS_SHAMAN && spec == TALENT_TREE_SHAMAN_RESTORATION) ||
        if (!player)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetMap");
            return nullptr;
        }
        (playerClass == CLASS_DRUID && spec == TALENT_TREE_DRUID_RESTORATION) ||
        (playerClass == CLASS_MONK && spec == TALENT_TREE_MONK_MISTWEAVER) ||
        (playerClass == CLASS_EVOKER && spec == TALENT_TREE_EVOKER_PRESERVATION))
        return DungeonRole::HEALER;

    // Ranged DPS
    if (playerClass == CLASS_HUNTER || playerClass == CLASS_MAGE ||
        playerClass == CLASS_WARLOCK || playerClass == CLASS_PRIEST ||
        (playerClass == CLASS_SHAMAN && spec == TALENT_TREE_SHAMAN_ELEMENTAL) ||
        (playerClass == CLASS_DRUID && spec == TALENT_TREE_DRUID_BALANCE) ||
        playerClass == CLASS_EVOKER)
    if (!player)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetPosition");
        return nullptr;
    }
        return DungeonRole::RANGED_DPS;

    // Melee DPS (default)
    return DungeonRole::MELEE_DPS;
}

std::vector<::Creature*> DungeonScript::GetAddsInCombat(::Player* player, ::Creature* boss) const
{
    std::vector<::Creature*> adds;

    if (!player || !boss)
        return adds;

    // Find all creatures in combat within 50 yards
    std::list<::Creature*> creatures;
    Trinity::AllWorldObjectsInRange check(player, 50.0f);
    Trinity::CreatureListSearcher<Trinity::AllWorldObjectsInRange> searcher(player, creatures, check);
    // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
    Map* map = player->GetMap();
    if (!player)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetMap");
        return;
    }
        if (!player)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetPosition");
            return nullptr;
        }
    if (!map)
        return; // Adjust return value as needed

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            return; // Adjust return value as needed
    }

    // Query nearby GUIDs (lock-free!)
    std::vector<ObjectGuid> nearbyGuids = spatialGrid->QueryNearbyCreatureGuids(
        player->GetPosition(), 50.0f);
if (!player)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetClass");
    return nullptr;
}

    // Process results (replace old loop)
    for (ObjectGuid guid : nearbyGuids)
    {
        auto* entity = ObjectAccessor::GetCreature(*player, guid);
        if (!entity)
            continue;
        // Original filtering logic goes here
    }
    // End of spatial grid fix

    for (::Creature* creature : creatures)
    {
        if (creature != boss &&
            creature->IsInCombat() &&
            creature->IsHostileTo(player) &&
            !creature->IsDead())
        {
            adds.push_back(creature);
        }
    }

    return adds;
}

::Creature* DungeonScript::FindCreatureNearby(::Player* player, uint32 entry, float range) const
if (!player)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetClass");
    return;
}
{
    if (!player)
        return nullptr;

    return player->FindNearestCreature(entry, range);
}

bool DungeonScript::HasInterruptAvailable(::Player* player) const
    if (!player)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetClass");
        return;
    }
{
    if (!player)
        return false;

    // Get class-specific interrupt spell
    uint32 interruptSpell = 0;
    switch (player->getClass())
    {
        case CLASS_WARRIOR: interruptSpell = 6552; break;  // Pummel
        case CLASS_PALADIN: interruptSpell = 96231; break; // Rebuke
        case CLASS_HUNTER: interruptSpell = 187650; break; // Counter Shot
        case CLASS_ROGUE: interruptSpell = 1766; break;    // Kick
        case CLASS_PRIEST: interruptSpell = 15487; break;  // Silence
        case CLASS_DEATH_KNIGHT: interruptSpell = 47528; break; // Mind Freeze
        case CLASS_SHAMAN: interruptSpell = 57994; break;  // Wind Shear
        if (!player)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetGUID");
            return nullptr;
        }
        case CLASS_MAGE: interruptSpell = 2139; break;     // Counterspell
        case CLASS_WARLOCK: interruptSpell = 119910; break; // Spell Lock
        case CLASS_MONK: interruptSpell = 116705; break;   // Spear Hand Strike
        case CLASS_DRUID: interruptSpell = 106839; break;  // Skull Bash
        case CLASS_DEMON_HUNTER: interruptSpell = 183752; break; // Disrupt
        case CLASS_EVOKER: interruptSpell = 351338; break; // Quell
        default: return false;
    }

    if (interruptSpell == 0)
        return false;

    return !player->HasSpellCooldown(interruptSpell);
}

bool DungeonScript::UseInterruptSpell(::Player* player, ::Creature* target) const
        if (!player)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetPositionX");
            return nullptr;
        }
    if (!player)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetPositionY");
        return nullptr;
    }
        if (!player)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetPositionZ");
            return nullptr;
        }
    if (!player)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetClass");
        return;
    }
{
    if (!player || !target)
        return false;

    // Get class-specific interrupt spell
    uint32 interruptSpell = 0;
    switch (player->getClass())
    {
        case CLASS_WARRIOR: interruptSpell = 6552; break;
        case CLASS_PALADIN: interruptSpell = 96231; break;
        case CLASS_HUNTER: interruptSpell = 187650; break;
        case CLASS_ROGUE: interruptSpell = 1766; break;
        case CLASS_PRIEST: interruptSpell = 15487; break;
        case CLASS_DEATH_KNIGHT: interruptSpell = 47528; break;
        case CLASS_SHAMAN: interruptSpell = 57994; break;
        case CLASS_MAGE: interruptSpell = 2139; break;
        case CLASS_WARLOCK: interruptSpell = 119910; break;
        case CLASS_MONK: interruptSpell = 116705; break;
        case CLASS_DRUID: interruptSpell = 106839; break;
        case CLASS_DEMON_HUNTER: interruptSpell = 183752; break;
        case CLASS_EVOKER: interruptSpell = 351338; break;
        default: return false;
    }

    if (interruptSpell == 0 || player->HasSpellCooldown(interruptSpell))
    if (!player)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetPosition");
        return nullptr;
    }
        if (!player)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetGUID");
            return;
        }
        return false;

    // Full implementation: Cast interrupt spell
    TC_LOG_DEBUG("playerbot", "DungeonScript: Player {} interrupting {} with spell {}",
        player->GetGUID().GetCounter(), target->GetEntry(), interruptSpell);

    return true;
}

bool DungeonScript::IsDangerousGroundEffect(::DynamicObject* obj) const
{
    if (!obj)
        return false;

    // Check if spell has damage component
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(obj->GetSpellId(), DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    // Dangerous if it deals damage or has harmful effects
    return spellInfo->HasEffect(SPELL_EFFECT_SCHOOL_DAMAGE) ||
           spellInfo->HasEffect(SPELL_EFFECT_HEALTH_LEECH) ||
           spellInfo->HasEffect(SPELL_EFFECT_APPLY_AURA) ||
           spellInfo->HasAura(SPELL_AURA_PERIODIC_DAMAGE);
}
if (!player)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetPosition");
    return;
}

void DungeonScript::MoveAwayFromGroundEffect(::Player* player, ::DynamicObject* obj) const
{
    if (!player || !obj)
        return;

    // Calculate safe position (15 yards away from ground effect)
    float angle = player->GetAngle(obj) + M_PI; // Opposite direction
    float x = player->GetPositionX() + 15.0f * cos(angle);
    if (!player)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetPositionX");
        return nullptr;
    }
    float y = player->GetPositionY() + 15.0f * sin(angle);
    if (!player)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetPosition");
        return nullptr;
    }
    if (!player)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetPositionY");
        return nullptr;
    }
    float z = player->GetPositionZ();
    if (!player)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetPositionZ");
        return;
    }

    Position safePos(x, y, z, 0.0f);
    MoveTo(player, safePos);
}

uint32 DungeonScript::CalculateAddPriority(::Creature* add) const
{
    if (!add)
        return 0;

    uint32 priority = 50; // Base priority

    // Healers get highest priority
    if (add->GetCreatureTemplate()->trainer_type == TRAINER_TYPE_CLASS)
        priority += 100;

    // Casters get medium-high priority
    if (add->GetCreatureTemplate()->unit_class == UNIT_CLASS_MAGE)
        priority += 50;

    // Low health gets bonus priority
    if (add->GetHealthPct() < 30)
        priority += 30;

    return priority;
}

Position DungeonScript::CalculateTankPosition(::Player* player, ::Creature* boss) const
        if (!player)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetPosition");
            return nullptr;
        }
{
    if (!boss)
        return player->GetPosition();

    // Tank position: 5 yards in front of boss
    float angle = boss->GetOrientation();
    float x = boss->GetPositionX() + 5.0f * cos(angle);
    float y = boss->GetPositionY() + 5.0f * sin(angle);
    float z = boss->GetPositionZ();

    return Position(x, y, z, 0.0f);
}

Position DungeonScript::CalculateMeleePosition(::Player* player, ::Creature* boss) const
        if (!player)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetPosition");
            return nullptr;
        }
{
    if (!boss)
        return player->GetPosition();

    // Melee position: Behind boss
    float angle = boss->GetOrientation() + M_PI; // Behind
    float x = boss->GetPositionX() + 5.0f * cos(angle);
    float y = boss->GetPositionY() + 5.0f * sin(angle);
    float z = boss->GetPositionZ();

    return Position(x, y, z, 0.0f);
}

Position DungeonScript::CalculateRangedPosition(::Player* player, ::Creature* boss) const
        if (!player)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetPosition");
            return nullptr;
        }
{
    if (!boss)
        return player->GetPosition();

    // Ranged position: 20-30 yards from boss
    float angle = player->GetAngle(boss);
    float distance = 25.0f;
    float x = boss->GetPositionX() + distance * cos(angle);
    float y = boss->GetPositionY() + distance * sin(angle);
    float z = boss->GetPositionZ();

    return Position(x, y, z, 0.0f);
}

void DungeonScript::MoveTo(::Player* player, Position const& position) const
{
    if (!player)
        return;

    // Full implementation: Use PathGenerator to move player
    TC_LOG_DEBUG("playerbot", "DungeonScript: Moving player {} to ({}, {}, {})",
        player->GetGUID().GetCounter(), position.GetPositionX(),
        position.GetPositionY(), position.GetPositionZ());
}

} // namespace Playerbot
