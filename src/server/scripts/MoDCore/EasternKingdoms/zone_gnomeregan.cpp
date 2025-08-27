/*
 * Copyright (C) 2020 Covenant
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* ScriptData
SDName: Gnomeregan
SD%Complete: Intro
SDComment: Quest Support: 27635, 28169
SDCategory: Gnomeregan
EndScriptData */

#include "Creature.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "Vehicle.h"
#include "MotionMaster.h"
#include "TemporarySummon.h"
#include "GameObject.h"

enum GnomeCreatureIds
{
    NPC_DECONTAMINATION_BUNNY = 46165,
    NPC_CLEAN_CANNON          = 46208,
    NPC_SAFE_TECHNICAN        = 46230,
    NPC_NEVIN_TWISTWRENCH     = 46293,
    NPC_IMUN_AGENT            = 47836
};

enum GnomeSpells
{
    SPELL_CANNON_BURST          = 86080,
    SPELL_DECONTAMINATE_STAGE_1 = 86075,
    SPELL_DECONTAMINATE_STAGE_2 = 86086,
    SPELL_IRRADIATE             = 80653,
    SPELL_EXPLOSION             = 30934
};

enum GnomeQuests
{
    QUEST_DECONTAMINATION              = 27635,
    QUEST_WITHDRAW_TO_THE_LOADING_ROOM = 28169
};

enum GnomeGossips
{
    GOSSIP_TORBEN      = 12104
};

enum GnomeMoves
{
    MOVE_IMUN_AGENT = 4783600
};

Position const SpawnPosition = { -4981.25f, 780.992f, 288.485f, 3.316f };

class npc_nevin_twistwrench : public CreatureScript
{
public:
    npc_nevin_twistwrench() : CreatureScript("npc_nevin_twistwrench") { }

    struct npc_nevin_twistwrenchAI : public ScriptedAI
    {
        npc_nevin_twistwrenchAI(Creature* creature) : ScriptedAI(creature) { }

        void MoveInLineOfSight(Unit * who) override
        {
            if (who->IsPlayer() && who->IsWithinDist(me, 10.f) && !who->HasAura(SPELL_IRRADIATE)
                && who->ToPlayer()->GetQuestStatus(QUEST_DECONTAMINATION) == QUEST_STATUS_NONE)
                who->CastSpell(who, SPELL_IRRADIATE, true);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_nevin_twistwrenchAI(creature);
    }

};

class npc_carvo_blastbolt : public CreatureScript
{
public:
    npc_carvo_blastbolt() : CreatureScript("npc_carvo_blastbolt") { }

    struct npc_carvo_blastboltAI : public ScriptedAI
    {
        npc_carvo_blastboltAI(Creature* creature) : ScriptedAI(creature) { }
    };
    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_carvo_blastboltAI(creature);
    }

    bool OnQuestAccept(Player* player, Creature* /*creature*/, Quest const* quest)
    {
        if (quest->GetQuestId() == QUEST_WITHDRAW_TO_THE_LOADING_ROOM)
        {
            if (TempSummon* agent = player->SummonCreature(NPC_IMUN_AGENT, SpawnPosition, TEMPSUMMON_TIMED_DESPAWN, 60s, 0))
            {
                agent->SetSpeed(MOVE_RUN, 1.0f);
                agent->SetUnitFlag(UnitFlags(UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC | UNIT_FLAG_UNINTERACTIBLE));
                agent->SetReactState(REACT_PASSIVE);
                agent->AI()->Talk(0, player);
                agent->GetMotionMaster()->MovePath(MOVE_IMUN_AGENT, false);
            }
        }

        return true;
    }
};

class npc_gnomeregan_torben : public CreatureScript
{
public:
    npc_gnomeregan_torben() : CreatureScript("npc_gnomeregan_torben") { }

    struct npc_gnomeregan_torbenAI : public ScriptedAI
    {
        npc_gnomeregan_torbenAI(Creature* creature) : ScriptedAI(creature) { }

        bool OnGossipHello(Player* player) override
        {
            AddGossipItemFor(player, GOSSIP_TORBEN, 1, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
            SendGossipMenuFor(player, player->GetGossipTextId(me), me->GetGUID());

            return true;
        }

        bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
        {
            uint32 action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);

            ClearGossipMenuFor(player);
            if (action == GOSSIP_ACTION_INFO_DEF + 1)
            {
                player->KilledMonsterCredit(NPC_NEVIN_TWISTWRENCH);
                player->TeleportTo(0, -5201.58f, 477.98f, 388.47f, 5.13f);
                CloseGossipMenuFor(player);
            }
            return true;
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_gnomeregan_torbenAI(creature);
    }
};

struct npc_multi_bot : public ScriptedAI
{
    npc_multi_bot(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        me->GetScheduler().Schedule(2s, [this](TaskContext context)
        {
            if (GameObject* gobject = me->FindNearestGameObject(203975, 5))
            {
                if (me->GetOwner()->IsPlayer())
                {
                    Talk(0);
                    gobject->SetGoState(GO_STATE_ACTIVE);
                    me->CastSpell(me, 79424, true);
                    me->CastSpell(me, 79422, true);
                }
            }

            context.Repeat();
        });
    }
};

void AddSC_MoDCore_zone_gnomeregan()
{
    new npc_nevin_twistwrench();
    new npc_carvo_blastbolt();
    new npc_gnomeregan_torben();
    RegisterCreatureAI(npc_multi_bot);
}

