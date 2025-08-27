#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellAuraEffects.h"
#include "Zone_BrackenhideHollow.h"

enum Spells
{
    SPELL_CONSUME = 377222,
    SPELL_DECAY_SPRAY = 376811,
    SPELL_VINE_WHIP = 377559,
    SPELL_GRASPING_VINES = 376934,
    SPELL_DECAY_SPRAY_SUMMON = 376797,//summon 1 slime need set to 4 
    SPELL_GUSHING_OOZE = 381770,//used by npc slime
    SPELL_BURST = 378057, //used by npc slime
};

enum Events
{
    EVENT_CONSUME = 1,
    EVENT_DECAY_SPRAY,
    EVENT_VINE_WHIP,
    EVENT_GRASPING_VINES,
    EVENT_DECAY_SPRAY_SUMMON,
    EVENT_RESET,
};

enum TreemouthNpcs
{
    NPC_DECAY_SLIME = 192481
};

enum GraspingVines
{
    DRAG_DURATION = 4000,
};

class boss_treemouth : public CreatureScript
{
public:
    boss_treemouth() : CreatureScript("boss_treemouth") {}

    struct boss_treemouthAI : public ScriptedAI
    {
        boss_treemouthAI(Creature* creature)
            : ScriptedAI(creature), events(), instance(nullptr), summons(creature) {
        }
        EventMap events; // <-- Hinzugefügt
        InstanceScript* instance; // <-- Hinzugefügt
        SummonList summons;

        void Reset() override
        {
            events.Reset();
            me->RemoveAurasDueToSpell(SPELL_VINE_WHIP);
        }

        void JustEngagedWith(Unit* who) override
        {
            ScriptedAI::JustEngagedWith(who);
            events.ScheduleEvent(EVENT_CONSUME, 6s);
            events.ScheduleEvent(EVENT_DECAY_SPRAY, 10s);
            events.ScheduleEvent(SPELL_VINE_WHIP, 15s);
            events.ScheduleEvent(EVENT_DECAY_SPRAY_SUMMON, 13s);
            instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me, 1);
        }

        void JustDied(Unit* /*killer*/) override
        {
            events.Reset();
        }

        void EnterEvadeMode(EvadeReason /*why*/) override
        {
            summons.DespawnAll();
            _EnterEvadeMode();
        }

        void UpdateAI(uint32 const diff) override
        {
            if (!UpdateVictim())
                return;

            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                case EVENT_CONSUME:
                    DoCastVictim(SPELL_CONSUME);
                    events.ScheduleEvent(EVENT_CONSUME, 6s);
                    break;
                case EVENT_DECAY_SPRAY:
                    DoCast(me->GetVictim(), SPELL_DECAY_SPRAY);
                    events.ScheduleEvent(EVENT_DECAY_SPRAY, 10s);
                    break;
                case SPELL_VINE_WHIP:
                    DoCastVictim(SPELL_VINE_WHIP);
                    events.ScheduleEvent(SPELL_VINE_WHIP, 15s);
                    break;
                case EVENT_DECAY_SPRAY_SUMMON:
                    DoCastVictim(SPELL_DECAY_SPRAY_SUMMON);
                    events.ScheduleEvent(SPELL_DECAY_SPRAY_SUMMON, 13s);
                    break;
                }
            }

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new boss_treemouthAI(creature);
    }
};

//npc=192481 decay slime
class npc_decaying_slime : public CreatureScript
{
public:
    npc_decaying_slime() : CreatureScript("npc_decaying_slime") {}

    struct npc_decaying_slimeAI : public ScriptedAI
    {
        npc_decaying_slimeAI(Creature* creature) : ScriptedAI(creature) {}

        void Reset() override
        {
            me->SetReactState(REACT_PASSIVE);
        }

        void EnterCombat(Unit* /*who*/)
        {
            DoZoneInCombat();
        }

        void JustDied(Unit* /*killer*/) override
        {
            me->DespawnOrUnsummon();
        }

        void UpdateAI(uint32 const /*diff*/) override
        {
            if (!UpdateVictim())
                return;

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            if (me->HealthBelowPct(50))
            {
                DoCast(me->GetVictim(), SPELL_GUSHING_OOZE);
            }
            else
            {
                DoCast(me, SPELL_BURST);
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_decaying_slimeAI(creature);
    }
};

void AddSC_boss_treemouth()
{
    new boss_treemouth();
    new npc_decaying_slime();
}
