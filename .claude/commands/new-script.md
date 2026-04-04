---
allowed-tools: Read, Write, Edit
description: Scaffold a new custom C++ script and register it in the script loader
paths: src/**/*.cpp, src/**/*.h, src/server/scripts/Custom/**
---

## Context

Custom scripts live in `src/server/scripts/Custom/`. Each script needs:
1. A `.cpp` file with script classes
2. A `void AddSC_<name>()` registration function at the bottom
3. A forward declaration and call in `custom_script_loader.cpp`

The script loader is at: `src/server/scripts/Custom/custom_script_loader.cpp`

## Arguments

$ARGUMENTS should contain the script name in snake_case (e.g., `my_new_feature`).

Optionally, the user may specify a script type after the name:
- `command` — CommandScript (chat commands)
- `player` — PlayerScript (player hooks)
- `world` — WorldScript (server hooks)
- `creature` — CreatureScript (creature AI)
- `spell` — SpellScript (spell effects)

Example: `/new-script my_feature command`

Default type if not specified: `player`

## Your task

1. Parse the script name and optional type from $ARGUMENTS
2. Create `src/server/scripts/Custom/<name>.cpp` with appropriate boilerplate based on type:

   **For `command` type:**
   ```cpp
   #include "Chat.h"
   #include "Player.h"
   #include "ScriptMgr.h"

   using namespace Trinity::ChatCommands;

   class <name>_commandscript : public CommandScript
   {
   public:
       <name>_commandscript() : CommandScript("<name>_commandscript") {}

       ChatCommandTable GetCommands() const override
       {
           static ChatCommandTable commandTable =
           {
               // { "commandname", HandleCommandFunction, rbac::RBAC_PERM_COMMAND_CUSTOM, Console::No },
           };
           return commandTable;
       }

       // static bool HandleCommandFunction(ChatHandler* handler, Optional<PlayerIdentifier> target)
       // {
       //     return true;
       // }
   };

   void AddSC_<name>()
   {
       new <name>_commandscript();
   }
   ```

   **For `player` type:**
   ```cpp
   #include "Player.h"
   #include "ScriptMgr.h"

   class <name>_playerscript : public PlayerScript
   {
   public:
       <name>_playerscript() : PlayerScript("<name>_playerscript") {}

       // void OnLogin(Player* player, bool firstLogin) override {}
       // void OnLogout(Player* player) override {}
   };

   void AddSC_<name>()
   {
       new <name>_playerscript();
   }
   ```

   **For `world` type:**
   ```cpp
   #include "ScriptMgr.h"

   class <name>_worldscript : public WorldScript
   {
   public:
       <name>_worldscript() : WorldScript("<name>_worldscript") {}

       // void OnStartup() override {}
       // void OnUpdate(uint32 diff) override {}
   };

   void AddSC_<name>()
   {
       new <name>_worldscript();
   }
   ```

   **For `creature` type:**
   ```cpp
   #include "Creature.h"
   #include "CreatureAI.h"
   #include "ScriptMgr.h"

   struct npc_<name> : public ScriptedAI
   {
       npc_<name>(Creature* creature) : ScriptedAI(creature) {}

       // void JustEngagedWith(Unit* who) override {}
       // void UpdateAI(uint32 diff) override {}
   };

   void AddSC_<name>()
   {
       RegisterCreatureAI(npc_<name>);
   }
   ```

   **For `spell` type:**
   ```cpp
   #include "Player.h"
   #include "ScriptMgr.h"
   #include "SpellAuraEffects.h"
   #include "SpellScript.h"

   // class spell_<name> : public SpellScript
   // {
   //     void HandleEffect(SpellEffIndex /*effIndex*/) {}
   //
   //     void Register() override
   //     {
   //         OnEffectHitTarget += SpellEffectFn(spell_<name>::HandleEffect, EFFECT_0, SPELL_EFFECT_DUMMY);
   //     }
   // };

   void AddSC_<name>()
   {
       // RegisterSpellScript(spell_<name>);
   }
   ```

3. Read `custom_script_loader.cpp`, then Edit it to add:
   - The forward declaration `void AddSC_<name>();` with the other declarations (before `AddCustomScripts()`)
   - The call `AddSC_<name>();` inside `AddCustomScripts()` body
4. Report what was created and remind the user to build with `ninja -j4 scripts` (or via Visual Studio)
