# RoleplayCore — Project Guide

## P0 — USE THE TRIAD (do not brute-force)
**You have live API access to ChatGPT (gpt-5.4) and Gemini (gemini-3.1-pro). USE THEM.**

### The Pipeline
```
1. DESIGN  → ChatGPT generates spec    → lands in AI_Studio/1_Inbox/
2. REVIEW  → ChatGPT reviews spec      → approved specs move to AI_Studio/2_Active_Specs/
3. BUILD   → Claude Code implements     → code/SQL/config written
4. REVIEW CYCLE (5-round, 3 reviewers):
   4a. ChatGPT reviews   → architecture/design      → fix issues
   4b. Gemini reviews    → correctness/edge cases   → fix issues
   4c. Claude API reviews → cold-read, impl bias    → fix issues
   4d. ChatGPT reviews   → verify fixes, coherence  → fix issues
   4e. Gemini reviews    → final seal (strictest)
5. USER    → human review of final artifact
```

### When to Call Each
| Trigger | Who | Command |
|---------|-----|---------|
| New feature / subsystem / architecture | ChatGPT | `python tools/api_architect/run_architect.py --prompt "..."` |
| Spec in inbox needs review before implementing | ChatGPT | `python tools/ai_studio/chatgpt_bridge.py --file SPEC.md` |
| Implementation done, non-trivial changes | Gemini | `python tools/ai_studio/orchestrator.py` |
| Unsure about an architecture decision | ChatGPT | Same as row 1 — ask before deciding |

### How Handoffs Work
- **ChatGPT → Claude Code**: Spec `.md` file lands in `AI_Studio/1_Inbox/`. Read it, claim it in Central Brain, implement it
- **Claude Code → Gemini**: After implementation, run orchestrator. It sends your diff + context to Gemini, gets back approval or rejection with specific issues
- **Gemini → Claude Code**: If rejected, fix the cited issues and re-run the audit. If approved, write handoff to `AI_Studio/Reports/Audits/`
- **Coordination**: Update `AI_Studio/0_Central_Brain.md` when starting/finishing work. Update `doc/session_state.md` for multi-tab coordination

### Exceptions (no API call needed)
Localized bug fixes, log parsing, build-loop, file cleanup, simple CLI ops, git operations.

### Self-Check
Before completing any session with non-trivial work, ask: *"Did I use the Triad, or did I brute-force this?"*

## THE TRIAD EVOLUTION DIRECTIVE (Core Philosophy)
At every step, ask: *"What would I do differently if I knew I had nearly unlimited potential to run AIs?"* You have agent swarms, subagents, custom skills, ChatGPT API, Gemini API, Cowork scheduled tasks, and massive parallel compute. Claude Code is the primary terminal — all other AIs are API endpoints. Never accept a standard approach if you can think of a smarter, faster, cheaper, or better way to leverage the swarm.

## THE "DIG DEEPER" MANDATE (3x Iteration Rule)
**ALWAYS try to "dig deeper" at least 3 times before reporting back.** Iterate, research, and push analysis 3 levels deep. If reports are massive, write to `AI_Studio/Reports/`.

## What This Is
TrinityCore-based WoW private server targeting **12.x / Midnight** client, specialized for **roleplay**. Custom systems, 5 databases (auth, characters, world, hotfixes, roleplay).

## CRITICAL RULES (Claude gets these wrong without them)
- **Building from Claude Code is allowed** — use `ninja -j32` via Bash (VS IDE also works)
- **DESCRIBE tables before writing SQL** — verify column names and count
- **No `item_template`** — use `hotfixes.item` / `hotfixes.item_sparse`
- **No `broadcast_text` in world** — use `hotfixes.broadcast_text`
- **`creature_template`**: column is `faction` (not FactionID), `npcflag` (bigint)
- Spells in `creature_template_spell` (cols: `CreatureID`, `Index`, `Spell`)

## Session Start — MANDATORY
See `.claude/rules/session-start.md`. In brief: Read `AI_Studio/0_Central_Brain.md` + `doc/session_state.md` + `todo.md` BEFORE responding. EXTRACT actionable items and show to user. Never silently drop items.

## Proactive Skill Reminders — MANDATORY
See `.claude/rules/skill-reminders.md`. The user should NEVER have to remember a slash command. Key: `/wrap-up` at end of session, `/check-logs` on crash/restart, `/lookup-*` for names without IDs.

## Work Style
**MANDATORY**: Always default to parallel execution. Hardware is not a constraint (16C/32T, 128GB DDR5, NVMe).
1. **2+ independent parts → parallel agents** — just do it
2. **2+ searches → fan out Explore agents** — never sequential
3. **Multiple errors → one agent per error category**
4. **Builds, long queries, server restarts → always background**

## Debugging — MANDATORY PIPELINE
See `.claude/rules/debugging.md`. 4-gate pipeline. No hypothesis without data. Never combine fixes.

## Completion Integrity — MANDATORY
See `.claude/rules/completion-integrity.md`. Never claim completion without tool output proving it.

## Multi-Tab Delegation — BLOCKING OBLIGATION
See `.claude/rules/multi-tab.md`. If task touches 2+ independent subsystems, MUST suggest tab split.

## Compaction Instructions
When compacting, ALWAYS preserve: (1) files modified this session, (2) current task/goal, (3) pending SQL or build actions, (4) spawned agents and findings. Drop: exploration results, failed approaches, verbose tool output.

## Release Gate — MANDATORY for Shipping
Before shipping any addon, tool, or app: run `/pre-ship <path>`. It runs automated checks (naming, non-ASCII, TOC, versions, docs, secrets) then spawns 3 adversarial review agents (noob, bully, security) in parallel. Writes `.claude/release-gate-status.json` which enforcement hooks read — `git push --tags` and `gh release create` are BLOCKED when gate != PASS. Full checklist: `memory/addon-building-checklist.md` (16 phases, ~130 items).

## Reference (loaded on-demand from `.claude/rules/`)
- **Project structure, build, DBs, systems, key files, tools** → `project-reference.md`
- **C++ coding conventions** → `coding-conventions.md`
# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

TrinityCore WoW server emulator (C++20, client 12.0.1+) with a large-scale Playerbot AI module (~636K lines, ~1,629 files) that adds AI-controlled player bots for a single-player MMORPG experience.

**Branches**: `playerbot-dev` (main development), `master` (base TrinityCore / PR target)

## Build Commands

### Configure (Windows, Visual Studio 17 2022)
```bat
configure_relwithdebinfo.bat   # Primary config (optimized + debug symbols)
configure_debug.bat            # Debug config
configure_release.bat          # Release config
```
CMake flag `-DBUILD_PLAYERBOT=1` enables the Playerbot module.

### Build
```bat
cmake --build build --config RelWithDebInfo --target worldserver
cmake --build build --config Debug --target worldserver
```
Build timeout: 30 minutes. The Playerbot module splits into multiple static libraries to avoid MSVC's 4GB COFF limit.

### Syntax Check (Linux/CI)
```bash
./check-playerbot-syntax.sh
```

## Critical Rules

- **Module-first**: ALL new code goes in `src/modules/Playerbot/`. Core changes (`src/server/`) need justification and must use hook/event patterns. Never refactor core wholesale.
- **TrinityCore master is golden**: Adapt our code to TrinityCore, never the reverse. Always use TrinityCore APIs; never bypass existing systems.
- **No stubs/TODOs**: Full, complete implementations always. No simplified placeholders.
- **Check data before coding**: Always check DB2/DBC/SQL data to avoid reimplementing what already exists.
- **Commit messages**: NEVER mention Claude/AI/Co-Authored-By. NEVER mention IDA/reverse-engineering/disassembly.
- **Target client**: WoW 12.0.1+ ONLY.
- **Performance target**: 100-500 concurrent bots, <10% server impact, <0.1% CPU per bot.

## Code Style

- **Formatting**: 4 spaces, no tabs, max 160 chars/line, latin1 charset for C/C++ files (see `.editorconfig`)
- **Naming**: PascalCase classes/methods, camelCase variables, `m_` prefix for members, UPPER_SNAKE for constants
- **Include order**: PCH -> own header -> project headers -> TrinityCore headers -> external libs -> stdlib
- **Threading**: `std::shared_mutex` for read-heavy, `std::mutex` for write-heavy, never hold locks across DB calls
- **Patterns**: Use TrinityCore's typed packet API, ObjectGuid system, smart pointers, RAII

## Architecture

### Module Entry Point
`PlayerbotModule` (singleton) in `src/modules/Playerbot/PlayerbotModule.h/cpp` — initializes all subsystems, registers with TrinityCore via `PlayerbotModuleAdapter`, provides `OnWorldUpdate(diff)` callback.

### Subsystem Registry
`Core/PlayerbotSubsystemRegistry` manages 30+ subsystems with priority-ordered init/update/shutdown. Subsystems registered via `Core/SubsystemAdapters.h/cpp`. Priority levels: CRITICAL (0) > HIGH (1) > NORMAL (2) > LOW (3).

Key subsystems by init order: BotAccountMgr (100) -> BotNameMgr (110) -> BotCharacterDistribution (120) -> BotWorldSessionMgr (130) -> BotPacketRelay (140) -> BotChatCommandHandler (150).

### AI Decision-Making
- `AI/BotAI` — main AI class with state machine (SOLO, COMBAT, DEAD, TRAVELLING, QUESTING, etc.)
- Strategies activated/deactivated dynamically for behavior switching
- `BehaviorPriorityManager` handles strategy selection
- Actions use command pattern: `ExecuteAction()` (sync) and `QueueAction()` (async, thread-safe)

### Class-Specific AI
`AI/ClassAI/ClassAI` extends BotAI with class-specific combat via `OnCombatUpdate()`. One subclass per WoW class. ClassAI ONLY handles combat; never controls movement.

### Threading Model
Worker threads produce actions via lock-free `BotActionQueue`. Main thread (`World::Update`) consumes and executes them in batches via `BotActionProcessor`. No shared mutable state between threads. `Threading/LockHierarchy.h` prevents deadlocks.

### Combat Event System
`Combat/CombatEvents.h` — event bus with typed events (spell, damage, healing, CC, threat). Priority levels: CRITICAL (interrupts, immediate) > HIGH (damage, 50ms) > MEDIUM (DoTs, 200ms) > LOW (state changes, 500ms). BotAI subscribes as `IEventHandler`.

### Movement System
`Movement/UnifiedMovementCoordinator` coordinates all movement. Sub-components: `BotMovementController` (state machine), pathfinding, road network graph, `MovementPriorityMapper` (arbitration). Migration in progress from direct `MotionMaster` calls to `BotMovementController`.

### Domain Event Buses
`Core/Events/GenericEventBus` — template-based event distribution with separate buses for: Groups, Loot, Quests, Auras, Cooldowns, Resources, Social, Auction, NPC, Instance, Profession.

### Hook System
`Core/PlayerBotHooks.h` — 8 hook points into TrinityCore (mainly Group.cpp). Observer pattern, nullable callbacks, <1μs overhead per hook.

### Database
- Module DB schema: `sql/playerbot/` (numbered migration files)
- `Database/PlayerbotDatabase` — module-specific DB
- `Database/PlayerbotMigrationMgr` — schema migrations
- `Database/PlayerbotCharacterDBInterface` — character DB access

### Bot Lifecycle
`Lifecycle/BotSpawner` (spawning/despawning), `BotFactory` (creation), `BotSaveController` (persistence). Instance bot pool: `Lifecycle/Instance/` with JIT factory and warm pool.

### Sessions
`Session/BotWorldSessionMgr` — session lifecycle. `Session/BotPacketRelay` — packet handling. `Session/AsyncBotInitializer` — async login flow.

### Dependencies
- CMake 3.24+, C++20, MySQL 9.4, Boost 1.74+
- Intel TBB (vendored fallback in `deps/tbb/`)
- MSVC 19.30+ (VS2022), GCC 11+, or Clang 14+

## File Modification Hierarchy

1. **PREFERRED** — Module-only (`src/modules/Playerbot/`), zero core modifications
2. **ACCEPTABLE** — Minimal core hooks via observer/event pattern only
3. **CAREFUL** — Core extension points with documented justification
4. **FORBIDDEN** — Core refactoring or breaking existing functionality

## MCP Tools

Research protocol: use BOTH `trinitycore` MCP (game data) AND `serena` (semantic code navigation) for any game mechanics research. Additional MCPs: `trinity-database` (MySQL), `sequential-thinking`, `github`, `context7`.
