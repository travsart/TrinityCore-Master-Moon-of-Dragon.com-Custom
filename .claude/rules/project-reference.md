---
paths: "src/**/*.cpp, src/**/*.h, src/**/*.hpp, sql/**/*.sql, tools/**/*.py, CMakeLists.txt, CMakePresets.json, _build_ps.ps1"
---

# VoxCore Project Reference

## Build

| Config | Dir | Use |
|---|---|---|
| `x64-Debug` | `out/build/x64-Debug/` | Compilation, debugging |
| **`x64-RelWithDebInfo`** | `out/build/x64-RelWithDebInfo/` | **Primary runtime** (17s startup vs 60s Debug) |

- **Build (preferred)**: `powershell.exe -ExecutionPolicy Bypass -File "_build_ps.ps1" debug 2>&1`
- **Scripts only**: `powershell.exe -ExecutionPolicy Bypass -File "_build_ps.ps1" debug scripts 2>&1`
- **RelWithDebInfo**: `powershell.exe -ExecutionPolicy Bypass -File "_build_ps.ps1" rel 2>&1`
- **Configure only**: `powershell.exe -ExecutionPolicy Bypass -File "_build_ps.ps1" debug configure 2>&1`
- **IMPORTANT**: Never use cmd.exe batch files from bash — they fail silently. Always use `_build_ps.ps1`
- **CMake presets**: Defined in `CMakePresets.json`. Use `--preset x64-Debug` or `--preset x64-RelWithDebInfo`
- **Key CMake options**: `SCRIPTS=static`, `ELUNA=ON`, `TOOLS=ON`
- **Compiler**: MSVC (VS 2026), Generator: Ninja, C++20
- **MySQL**: `C:/Program Files/MySQL/MySQL Server 8.0/bin/mysql.exe` — root/admin

## Databases (5 total)

| DB | Purpose |
|---|---|
| `auth` | Accounts, RBAC permissions |
| `characters` | Player data |
| `world` | Game world data (creatures, items, spells, etc.) |
| `hotfixes` | Client hotfix overrides |
| **`roleplay`** | Custom: `creature_extra`, `creature_template_extra`, `custom_npcs`, `server_settings` |

## Project Structure

```
src/server/
  game/
    RolePlay/              # sRoleplay singleton — central custom system
    Companion/             # sCompanionMgr singleton — companion squad AI
    Hoff/                  # Utility class (FindMapCreature, movement calc)
    Entities/Creature/CreatureOutfit.*  # NPC outfit/appearance overlay
    Craft/                 # Crafting system
    LuaEngine/             # Eluna scripting integration
  scripts/
    Custom/                # ALL custom scripts (see Custom Systems below)
      custom_script_loader.cpp  # Entry point: AddCustomScripts()
      Companion/           # CompanionAI + commands + scripts
      RolePlayFunction/    # Display/ (.display) + Effect/ (.effect)
    Commands/
      cs_customnpc.cpp     # .customnpc / .cnpc commands
  database/
    Database/Implementation/RoleplayDatabase.*  # 5th DB connection
sql/
  RoleplayCore/            # One-time setup scripts
  updates/                 # Incremental updates (YYYY_MM_DD_NN_<db>.sql)
```

## Custom Systems

### Core Singletons
1. **Roleplay (`sRoleplay`)** — `src/server/game/RolePlay/` — creature extras, custom NPCs, player extras. Loaded via `sRoleplay->LoadAllTables()`
2. **Companion Squad (`sCompanionMgr`)** — `src/server/game/Companion/` — DB-driven NPC companions with `.comp` commands, role-based AI (tank/healer/DPS), formation movement. Entries 500001-500005
3. **Custom NPC (`.cnpc`)** — `src/server/scripts/Commands/cs_customnpc.cpp` — player-race NPCs with custom equipment/appearance. Config: `CreatureTemplateIdStart = 400000`

### Script Systems (all in `src/server/scripts/Custom/`)
4. **Visual Effects (`.effect`)** — `Noblegarden::EffectsHandler` — SpellVisualKit persistence, late-join sync
5. **Display/Transmog (`.display`)** — `RoleplayCore::DisplayHandler` — per-slot appearance overrides
6. **Transmog Outfits** — `CMSG_TRANSMOG_OUTFIT_*` handling for 12.x wardrobe (ARCHIVED — reimplemented externally)
7. **Player Morph (`.wmorph`/`.wscale`/`.remorph`)** — `player_morph_scripts.cpp` — persistent player morph/scale
8. **Misc Scripts** — `spell_dragonriding.cpp`, `item_toy_scripts.cpp`, `spell_wormhole_generators.cpp`, `spell_clear_transmog.cpp`, `free_share_scripts.cpp`

## Key Files

| File | Why |
|---|---|
| `src/server/game/RolePlay/RolePlay.h` | Central roleplay API — read this first |
| `src/server/scripts/Custom/custom_script_loader.cpp` | Script registration entry point |
| `src/server/game/Entities/Creature/CreatureOutfit.h` | NPC appearance overlay system |
| `src/server/game/Accounts/RBAC.h` | Permission constants (custom range 1000+) |
| `src/server/worldserver/worldserver.conf.dist` | All config keys including custom ones |

## Adding a New Custom Script

1. Create `.cpp` (and optionally `.h`) in `src/server/scripts/Custom/`
2. Define `void AddSC_<name>()` at the bottom
3. Add the declaration + call in `custom_script_loader.cpp`
4. If it needs new RBAC perms, add to `RBAC.h` and `sql/RoleplayCore/1. auth db.sql`
5. Build with `ninja -j32 scripts`

## Server Runtime & Logs

- **Primary runtime**: `out/build/x64-RelWithDebInfo/bin/RelWithDebInfo/`
- **Logs**: `Server.log`, `DBErrors.log`, `Debug.log`, `GM.log`, `Bnet.log`, `PacketLog/`
- **worldserver.conf**: in runtime dir (NOT in source tree)
- **MySQL**: UniServerZ 9.5.0 (bundled), root/admin

## Tools

- **MCP servers**: `wago-db2` (DB2 CSV queries), `mysql` (direct DB access), `codeintel` (C++ symbol lookup)
- **LSP plugins**: `clangd-lsp` (C++), `lua-lsp` (Lua), `github` (PRs/issues)
- **19 slash commands**: `/build-loop`, `/check-logs`, `/parse-errors`, `/apply-sql`, `/soap`, `/lookup-spell`, `/lookup-item`, `/lookup-creature`, `/lookup-area`, `/lookup-faction`, `/lookup-emote`, `/lookup-sound`, `/decode-pkt`, `/parse-packet`, `/new-script`, `/new-sql-update`, `/smartai-check`, `/todo`, `/wrap-up`
- **External repos**: wago (`wago/`), tc-packet-tools (`tools-dev/tc-packet-tools/`), code-intel (`tools-dev/code-intel/`), claude-skills (`tools-dev/claude-skills/`)
- **External tools**: `ExtTools/` (WowPacketParser, wow.tools.local, DBC2CSV, Arctium, etc.)
- **GitHub**: `VoxCore84/RoleplayCore` (private), `gh` CLI authenticated
