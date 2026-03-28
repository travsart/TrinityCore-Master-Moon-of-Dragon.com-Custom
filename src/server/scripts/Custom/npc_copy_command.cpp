#include "Chat.h"
#include "ChatCommand.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "RBAC.h"
#include "ScriptMgr.h"

using namespace Trinity::ChatCommands;

class npc_copy_commandscript : public CommandScript
{
public:
    npc_copy_commandscript() : CommandScript("npc_copy_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable npcCopyTable =
        {
            { "", HandleNpcCopyCommand, rbac::RBAC_PERM_COMMAND_NPC_ADD, Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "npc copy", npcCopyTable },
        };

        return commandTable;
    }

    static bool HandleNpcCopyCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        Creature* source = handler->getSelectedCreature();
        if (!source)
        {
            handler->SendSysMessage("Select a creature to copy.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        ObjectGuid::LowType sourceGuid = source->GetSpawnId();
        if (!sourceGuid)
        {
            handler->SendSysMessage("Cannot copy a temporary summon.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 entry = source->GetEntry();
        CreatureTemplate const* cInfo = sObjectMgr->GetCreatureTemplate(entry);
        if (!cInfo)
        {
            handler->PSendSysMessage("Creature template %u not found.", entry);
            handler->SetSentErrorMessage(true);
            return false;
        }

        Map* map = player->GetMap();

        // --- Create base creature at player position (same flow as .npc add) ---
        Creature* creature = Creature::CreateCreature(entry, map, player->GetPosition());
        if (!creature)
        {
            handler->SendSysMessage("Failed to create creature.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        PhasingHandler::InheritPhaseShift(creature, player);
        creature->SaveToDB(map->GetId(), { map->GetDifficultyID() });
        ObjectGuid::LowType newGuid = creature->GetSpawnId();

        // Remove transient creature — we'll respawn from DB after syncing overrides
        creature->CleanupsBeforeDelete();
        delete creature;

        // --- Copy per-spawn DB overrides (position stays at player's location) ---
        WorldDatabase.DirectPExecute(
            "UPDATE creature dst JOIN creature src ON src.guid = {} SET "
            "dst.spawnDifficulties = src.spawnDifficulties, "
            "dst.phaseUseFlags = src.phaseUseFlags, "
            "dst.PhaseId = src.PhaseId, "
            "dst.PhaseGroup = src.PhaseGroup, "
            "dst.terrainSwapMap = src.terrainSwapMap, "
            "dst.modelid = src.modelid, "
            "dst.equipment_id = src.equipment_id, "
            "dst.spawntimesecs = src.spawntimesecs, "
            "dst.wander_distance = src.wander_distance, "
            "dst.MovementType = src.MovementType, "
            "dst.npcflag = src.npcflag, "
            "dst.unit_flags = src.unit_flags, "
            "dst.unit_flags2 = src.unit_flags2, "
            "dst.unit_flags3 = src.unit_flags3, "
            "dst.ScriptName = src.ScriptName, "
            "dst.StringId = src.StringId, "
            "dst.size = src.size "
            "WHERE dst.guid = {}",
            sourceGuid, newGuid);

        // Copy creature_addon (PathId=0 since waypoint coords are position-relative)
        WorldDatabase.DirectPExecute(
            "INSERT IGNORE INTO creature_addon "
            "(guid, PathId, mount, StandState, AnimTier, VisFlags, SheathState, PvPFlags, "
            "emote, aiAnimKit, movementAnimKit, meleeAnimKit, visibilityDistanceType, auras) "
            "SELECT {}, 0, mount, StandState, AnimTier, VisFlags, SheathState, PvPFlags, "
            "emote, aiAnimKit, movementAnimKit, meleeAnimKit, visibilityDistanceType, auras "
            "FROM creature_addon WHERE guid = {}",
            newGuid, sourceGuid);

        // Copy creature_movement_override
        WorldDatabase.DirectPExecute(
            "INSERT IGNORE INTO creature_movement_override "
            "(SpawnId, HoverInitiallyEnabled, Chase, `Random`, InteractionPauseTimer) "
            "SELECT {}, HoverInitiallyEnabled, Chase, `Random`, InteractionPauseTimer "
            "FROM creature_movement_override WHERE SpawnId = {}",
            newGuid, sourceGuid);

        // Copy creature_static_flags_override
        WorldDatabase.DirectPExecute(
            "INSERT IGNORE INTO creature_static_flags_override "
            "(SpawnId, DifficultyId, StaticFlags1, StaticFlags2, StaticFlags3, StaticFlags4, "
            "StaticFlags5, StaticFlags6, StaticFlags7, StaticFlags8) "
            "SELECT {}, DifficultyId, StaticFlags1, StaticFlags2, StaticFlags3, StaticFlags4, "
            "StaticFlags5, StaticFlags6, StaticFlags7, StaticFlags8 "
            "FROM creature_static_flags_override WHERE SpawnId = {}",
            newGuid, sourceGuid);

        // Copy roleplay creature_extra
        RoleplayDatabase.DirectPExecute(
            "INSERT IGNORE INTO creature_extra "
            "(guid, scale, id_creator_bnet, id_creator_player, id_modifier_bnet, id_modifier_player, "
            "created, modified, phaseMask, displayLock, displayId, nativeDisplayId, "
            "genderLock, gender, swim, gravity, fly) "
            "SELECT {}, scale, id_creator_bnet, id_creator_player, id_modifier_bnet, id_modifier_player, "
            "created, NOW(), phaseMask, displayLock, displayId, nativeDisplayId, "
            "genderLock, gender, swim, gravity, fly "
            "FROM creature_extra WHERE guid = {}",
            newGuid, sourceGuid);

        // --- Sync in-memory CreatureData with source overrides ---
        CreatureData const* sourceData = sObjectMgr->GetCreatureData(sourceGuid);
        if (sourceData)
        {
            CreatureData& newData = sObjectMgr->NewOrExistCreatureData(newGuid);
            newData.display = sourceData->display;
            newData.equipmentId = sourceData->equipmentId;
            newData.wander_distance = sourceData->wander_distance;
            newData.currentwaypoint = 0;
            newData.curHealthPct = sourceData->curHealthPct;
            newData.movementType = sourceData->movementType;
            newData.npcflag = sourceData->npcflag;
            newData.unit_flags = sourceData->unit_flags;
            newData.unit_flags2 = sourceData->unit_flags2;
            newData.unit_flags3 = sourceData->unit_flags3;
            newData.size = sourceData->size;
            newData.phaseUseFlags = sourceData->phaseUseFlags;
            newData.phaseId = sourceData->phaseId;
            newData.phaseGroup = sourceData->phaseGroup;
            newData.terrainSwapMap = sourceData->terrainSwapMap;
            newData.spawntimesecs = sourceData->spawntimesecs;
            newData.spawnDifficulties = sourceData->spawnDifficulties;
            newData.scriptId = sourceData->scriptId;
            newData.StringId = sourceData->StringId;
        }

        // --- Spawn the creature from DB ---
        creature = Creature::CreateCreatureFromDB(newGuid, map, true, true);
        if (!creature)
        {
            handler->SendSysMessage("Failed to load copied creature from database.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // --- Live-sync per-spawn addon visuals ---
        // The addon SQL insert ensures persistence, but ObjectMgr's addon cache
        // wasn't updated at runtime. Apply source addon state directly.
        CreatureAddon const* srcAddon = sObjectMgr->GetCreatureAddon(sourceGuid);
        if (srcAddon)
        {
            // Clear template addon auras first (per-spawn addon replaces them entirely)
            CreatureAddon const* templateAddon = sObjectMgr->GetCreatureTemplateAddon(entry);
            if (templateAddon)
                for (uint32 aura : templateAddon->auras)
                    creature->RemoveAurasDueToSpell(aura);

            if (srcAddon->mount)
                creature->Mount(srcAddon->mount);
            creature->SetStandState(UnitStandStateType(srcAddon->standState));
            creature->ReplaceAllVisFlags(UnitVisFlags(srcAddon->visFlags));
            creature->SetAnimTier(AnimTier(srcAddon->animTier), false);
            creature->SetSheath(SheathState(srcAddon->sheathState));
            creature->ReplaceAllPvpFlags(UnitPVPStateFlags(srcAddon->pvpFlags));
            if (srcAddon->emote)
                creature->SetEmoteState(Emote(srcAddon->emote));
            creature->SetAIAnimKitId(srcAddon->aiAnimKit);
            creature->SetMovementAnimKitId(srcAddon->movementAnimKit);
            creature->SetMeleeAnimKitId(srcAddon->meleeAnimKit);
            if (srcAddon->visibilityDistanceType != VisibilityDistanceType::Normal)
                creature->SetVisibilityDistanceOverride(srcAddon->visibilityDistanceType);

            for (uint32 aura : srcAddon->auras)
                if (!creature->HasAura(aura))
                    creature->AddAura(aura, creature);
        }

        sObjectMgr->AddCreatureToGrid(sObjectMgr->GetCreatureData(newGuid));

        handler->PSendSysMessage("Copied '%s' (entry %u). New spawn GUID: %s",
            cInfo->Name.c_str(), entry, std::to_string(newGuid).c_str());

        return true;
    }
};

void AddSC_npc_copy_command()
{
    new npc_copy_commandscript();
}
