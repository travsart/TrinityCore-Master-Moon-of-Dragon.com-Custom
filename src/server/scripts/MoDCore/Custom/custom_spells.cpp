#include "ScriptMgr.h"
#include "SpellScript.h"
#include "Player.h"
#include "Creature.h"

// 42662 shadowy-figure
// 0 -11131.939453 546.599243 70.376770 0.276324

// 42655 helix-gearbreaker
// 0 -11127.518555 547.733093 70.422829 3.392655

//INSERT INTO spell_script_names (spell_id, ScriptName) VALUES
//(79528, 'spell_potion_of_shrouding');

class spell_potion_of_shrouding : public SpellScript
{
    void HandleAfterCast()
    {
        if (Player* player = GetCaster()->ToPlayer())
        {
            uint32 npcHelix = 42655;
            uint32 npcShadowy = 42662;

            // Koordinaten
            float helixX = -11127.5f, helixY = 547.73f, helixZ = 70.42f, helixO = 3.39f;
            float shadowyX = -11131.9f, shadowyY = 546.59f, shadowyZ = 70.37f, shadowyO = 0.27f;

            // Spieler nah genug an beiden Positionen?
            if (player->IsWithinDist2d(helixX, helixY, 3.0f) && player->IsWithinDist2d(shadowyX, shadowyY, 3.0f))
            {
                Creature* helix = player->SummonCreature(npcHelix, helixX, helixY, helixZ, helixO);
                Creature* shadowy = player->SummonCreature(npcShadowy, shadowyX, shadowyY, shadowyZ, shadowyO);

                if (helix)
                    helix->AI()->Talk(0); // sofort

                if (shadowy)
                {
                    shadowy->AddDelayedEvent(6000, [shadowy]()
                        {
                            if (shadowy)
                                shadowy->AI()->Talk(0); // nach 6 Sekunden
                        });
                }
            }
        }
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_potion_of_shrouding::HandleAfterCast);
    }
};

void AddSC_custom_spell_scripts()
{
    new spell_potion_of_shrouding();
}
