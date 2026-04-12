---
spec_id: TRIAD-VOXSNIFFER-V2-ENHANCEMENTS-V7
title: VoxSniffer v2.0 Enhancement Spec (V7)
status: Review
priority: P1
date: 2026-03-13
architect: ChatGPT (V1), Claude Code (V2-V7)
intended_implementer: Claude Code
workflow: VoxCore Triad
target_tree: tools/publishable/VoxSniffer/
---

# VoxSniffer v2.0 Enhancement Spec (V7)

## 0) Revision Notes

**Prior versions**: V4 fixed 22+ issues from V1-V3 review cycles. V5 fixed 1 CRITICAL + 12 HIGH + 15 MEDIUM from V4 reviews. V6 fixed 7 HIGH + 9 MEDIUM from V5 reviews. See git history for full V4/V5/V6 changelogs.

V7 addresses findings from the V6 review cycle (R2 Gemini: 1 CRITICAL + 1 MEDIUM + 1 LOW; R3 Claude: 4 HIGH + 10 MEDIUM + 7 LOW).

**CRITICAL fix:**
- **SeedFromDB moved to ADDON_LOADED**: `PLAYER_LOGIN` does not fire on `/reload`, causing `totalFlushedRecords` to remain 0 while `VoxSnifferDB` retains all data — defeating the budget check. Fix: register `ADDON_LOADED` event, check `if addonName == "VoxSniffer"`, call `Schema.Validate()` + `Cfg.Init()` + `FM.SeedFromDB()` there. `PLAYER_LOGIN` still handles module enable, auto-start, and `LC.Refresh()` (which need the player in the world). Both events fire on fresh login; only `ADDON_LOADED` fires on `/reload`.

**HIGH fixes:**
- **get_location() defined ONLY in utils.py**: Removed the duplicate code block from `location_normalizer.py` (Section 3.12 / Phase 7). The function body appears once in `normalize/utils.py`. `location_normalizer.py` imports it: `from normalize.utils import get_location`.
- **LocationNormalizer defined as a standalone enricher**: `location_normalizer.py` contains a `LocationNormalizer` class with an `enrich(records)` method. It is NOT added to `ALL_NORMALIZERS` (which requires `BaseNormalizer` subclass with `obs_type` + `filter_records()`). Instead, `normalize_all()` calls `LocationNormalizer().enrich(records)` as a pre-processing step on all records before domain-specific normalization.
- **EventBus introduced as existing V1 component**: `Core/EventBus.lua` already exists in V1 (loaded via `VoxSniffer.toc`). Provides `NS.EventBus.Subscribe(event, callback)` and `NS.EventBus.Publish(event, data)`. Callbacks fire synchronously (inline, same frame, subscription order). File structure table updated to list EventBus.lua as existing.
- **Step ordering note added**: Step 8 (Phase 5 EventBus subscription) is listed before Step 10 (Phase 6 EventBus publisher). The subscription is inert until Step 10 completes — this is harmless (subscribing before the publisher exists just means no events fire yet). Explicitly noted in Section 10.

**MEDIUM fixes:**
- **totalFlushed vs totalFlushedRecords clarified**: V1 has `local totalFlushed` used for per-flush-cycle logging. V2 adds `local totalFlushedRecords` as the persistent budget counter. Phase 6 insertion point now shows surrounding V1 context lines for clarity.
- **Ghost tracker.OnFlush removed from V5 revision notes**: V6 replaced `tracker.OnFlush(session)` with EventBus subscription. The V5 revision note incorrectly referenced the old API — removed.
- **EB alias declared**: Phase 5 now shows `local EB = NS.EventBus` at the top of the UnitScanner additions block. Phase 6 uses `NS.EventBus.Publish()` (fully qualified, since FlushManager is a core module — no alias needed).
- **Log alias confirmed in FlushManager**: `local Log = NS.Log` already exists at file scope in V1 FlushManager.lua (line 9). Phase 1 stubs and Phase 6 replacement do NOT re-declare it — they use the existing file-scope alias for `Log.Warn()`/`Log.Error()` calls.
- **Duplicate comment removed**: The comment "GetInstanceInfo returns 8+ values; we only use positions 1-4" appeared twice in LC.Refresh pseudocode. Removed the duplicate.
- **Phase 1 stub insertion point specified**: "Add after the existing `FM.Reset()` function at the end of FlushManager.lua (after the V1 module-level code)."
- **scanner.ResetState module exposure confirmed**: UnitScanner's module table is `scanner` (UnitScanner.lua line 14: `local scanner = {}`). When `NS.modules["UnitScanner"]` is set, it references the same table. `NS.ResetAllModuleState()` iterates `NS.modules` and calls `mod.ResetState()` on each, which reaches `scanner.ResetState()` directly.
- **EventBus synchronous guarantee stated**: Added to Section 3.14: "EventBus.Publish() calls all subscribers synchronously (inline). Callbacks execute in subscription order within the same frame. There is no deferred/async execution."
- **warnLogged re-fire on resume documented**: After resume at 92%, the 80% warn fires again on the next flush. This is intentional — the user should be re-warned after manually resuming into a high-fill state.
- **pcall comment clarified**: Changed from "positions 1-4" to "return values 1-4 of GetInstanceInfo (pcall prepends the success bool)."
- **Phase 6 insertion context shows V1 lines**: The `FM.FlushAll(reason)` insertion point now shows the surrounding V1 code (`local totalFlushed = totalFlushed + totalRecords` context) so the implementer can locate the exact spot.
- **Export single-frame performance noted**: `/vs export objects` iterates all chunks in a single frame. For expected data volumes (<100K records), this completes well within the 6s script timeout. If datasets approach budget limits, consider chunked coroutine processing in a future version.

**LOW fixes:**
- **pcall pattern caveat added**: Section 3.5 now notes that `ok and val or fallback` is unsafe if `val` can be `false`. All current uses return strings/numbers/tables so the pattern is safe, but future APIs returning boolean should use `if ok then ... else ... end`.
- **GM2 nodeId clarified**: The value stored at `[encodedCoord]` is `goEntry` (the `gameobject_template.entry` ID).
- **BasicFrameTemplateWithInset TitleText noted**: `TitleText` is the standard child of `BasicFrameTemplateWithInset` on retail. Added to "Needs validation" API list for 12.x confirmation.
- **select(2, GetBuildInfo()) comment improved**: Clarified that `select(2, ...)` returns all values from position 2 onward, but assigning to a single variable captures only the first (build number).
- **GetAddOnInfo return values documented**: Comment notes `GetAddOnInfo(i)` returns `(name, title, notes, loadable, reason, security, newVersion)` — we capture only `name`.
- **Scheduler behavior during loading screens documented**: The WoW OnUpdate timer (which drives Scheduler) pauses during loading screens. LC.Refresh() is also called from ZONE_CHANGED_NEW_AREA, which fires after load completes. Between these two callers, the cache is always refreshed promptly.
- **encode_coord overflow comment added**: Without clamping, `encode_coord(1.0, 1.0)` produces `100,010,000` which overflows typical integer column limits. Clamping to 0.9999 produces `99,999,999` (max safe value). Edge-of-map nodes shift inward by ~0.01%.
- **Fragile line number references removed**: Replaced `Constants.lua:28`, `FlushManager.lua:35`, etc. with constant/function names only. Line numbers shift as code evolves.

**Self-audit fixes (noob/bully/security pass):**
- **Cfg.Init argument fixed**: `Cfg.Init(VoxSnifferDB.config)` not `Cfg.Init(VoxSnifferDB)`. V1 passes `.config` sub-table; passing entire DB would corrupt config with database-level keys.
- **LocationNormalizer is NOT a BaseNormalizer**: It lacks `obs_type` and `filter_records()`. Changed to standalone enricher with `enrich(records)` method, called as pre-processing step in `normalize_all()` before domain normalizers.
- **`scanner` not `tracker`**: UnitScanner.lua uses `local scanner = {}` (line 14), not `tracker`. All Phase 5 code blocks, revision notes, and acceptance criteria updated.
- **Log alias not re-declared**: `local Log = NS.Log` already exists at FlushManager.lua file scope (V1 line 9). Phase 1 stubs and Phase 6 replacement block do NOT re-declare it.
- **Config.lua `debug` table default added**: `debug = { externalLogSink = false }` added to DEFAULTS so the DLAPI sink guard (`cfg.debug.externalLogSink`) is not dead code.
- **`encode_coord` import at module level**: Moved `import math` from function body to module-level imports in `gm2_exporter.py`.
- **ShowExportFrame placement specified**: "Add in VoxSniffer.lua before the slash command handler."

## 1) Target & Scope

**Target addon tree**: `tools/publishable/VoxSniffer/` (the shipped v1.0.0 addon)
**NOT**: `tools-dev/VoxSniffer/` (stale prototype, ignore)

**Python pipeline**: `tools/voxsniffer/` (parsers/, normalize/, exporters/, cli.py)

**Goal**: Improve capture quality, contextual metadata, scanner robustness, and developer diagnostics. Stay zero-dependency. Do NOT redesign the UI, adopt frameworks, or turn VoxSniffer into a kitchen sink.

## 2) Tier Matrix

### Tier 1: High Value, Low Risk (implement first)

| # | Enhancement | Scope |
|---|-------------|-------|
| T1.1 | **Centralize location gathering** — extract duplicated player-position code from 5 modules + ObjectTracker vignette path into `Core/LocationContext.lua`. Scheduler-driven 1s refresh. | M |
| T1.2 | **Enrich location context** — subzone, instance name/type/difficulty. `lc` field in envelopes. Copy semantics (no aliasing). | S |
| T1.3 | **Centralize item link parsing** — extract from VendorCapture, LootCapture, QuestCapture into `Core/ItemLinkParser.lua`. | S |
| T1.4 | **DevTool soft integration** — `/vs inspect <target>` pushes to DevTool if installed. | S |
| T1.5 | **_DebugLog soft integration** — optional DLAPI sink in Logging.lua, gated. | S |
| T1.6 | **Scan environment at session start** — nameplateMaxDistance CVAR, client build, addon names, enabled modules. | S |

### Tier 2: High Value, Medium Risk

| # | Enhancement | Scope |
|---|-------------|-------|
| T2.1 | **Item link metadata enrichment** — bonus IDs, upgrade level. BLOCKED until in-game validation (Step 9). | M |
| T2.2 | **UnitScanner lifecycle hardening** — generation counter, cleanup on REMOVED, churn diagnostics. | M |
| T2.3 | **GatherMate2 export (Python-only)** — `gm2_exporter.py` with file-based classification (optional CSV). In-game `/vs export objects` exports raw text. | M |
| T2.4 | **SavedVariables size budget** — persistent record counter, seeded from DB on load. Warn 80%, pause 95%. Resume clears pause only. | M |
| T2.5 | **Schema v2 migration** — bump SCHEMA_VERSION, `lc` field, `diagnostics` section. Python handles v1/v2 coexistence. | S |

### Tier 3: Nice to Have (defer)

T3.1 HandyNotes export, T3.2 Hidden tooltip scanner, T3.3 BtWQuests cross-ref, T3.4 ATT delta, T3.5 CVAR resampling.

### Rejected

Ace3/libDFramework (zero-dep violation), OneWoW (no value), runtime ATT/BtWQuests embedding (Python-only), WowLua integration (scope creep), direct GM2 SV writes (corruption risk), in-game GM2 classification (requires world DB).

## 3) Architectural Decisions

### 3.1 Stay zero-dependency
All features must work with no external addons. Soft integrations probe `_G` globals and no-op when absent.

### 3.2 LocationContext replaces VoxSniffer.lua's private cache

**Current V1 state**: VoxSniffer.lua has `local cachedMapId`, `local cachedPos`, `local cachedZone` (lines 54-56) refreshed by `RefreshPlayerContext()` (lines 58-66), called every 1s from OnUpdate (lines 454-456) and on ZONE_CHANGED_NEW_AREA (line 443). All are **private locals**.

Additionally, 5 modules + ObjectTracker vignette path gather player position inline:
- VendorCapture.lua:46-50, LootCapture.lua:~69, ObjectTracker.lua:51-55 (mouseover), ObjectTracker.lua:118-123 (vignette), GossipCapture.lua:110-116, QuestCapture.lua:199-205

**V2 design**: `Core/LocationContext.lua` loads after `Core/FlushManager.lua` in TOC. Registers `NS.LocationContext = {}`. Provides `Get()` (returns new shallow-copy table per call) and `Refresh()` (immediate cache update). Registers a 1s Scheduler callback at file-load time. This is safe because Scheduler.lua loads earlier in TOC order and `NS.Scheduler.Register` is available. Core services use load-time registration; modules use Enable/Disable registration.

**mapId=0 resilience**: If `C_Map.GetBestMapForUnit("player")` returns 0 or nil (e.g., during zone transition, loading screen), `LC.Refresh()` returns early without modifying the cache. Previous valid data is preserved. The Scheduler's next 1s tick retries and corrects within one cycle. Initial state is `{mapId=0, pos=nil, zone=""}` until the first successful refresh.

VoxSniffer.lua **removes** its private cache (lines 54-56, 58-66, 447-457) and replaces the ZONE_CHANGED handler call (line 443) with `NS.LocationContext.Refresh()`. The 6 inline position blocks are **removed** from modules.

### 3.3 MovementTracker is EXCLUDED from LocationContext refactor
MovementTracker.lua's `GetUnitMapPosition(unit)` and `EstimateUnitPosition(unit)` sample **NPC positions**, not player position. Replacing with LocationContext would break waypoint tracking. Left untouched.

### 3.4 Copy semantics and caller nil-safety
`MakeEnvelope()` creates TWO independent pos copies from `LC.Get()`. `envelope.pos` and `envelope.lc.pos` are separate table objects. Truthiness check is on `lc.pos` (table), not `lc.pos.x` (number) — coordinates of 0.0 are preserved correctly.

**Caller nil-safety**: All V1 module callers already guard `MakeEnvelope()` returns with `if not envelope then return end`. This is the established V1 convention (ObjectTracker.lua:92, and identical pattern in UnitScanner, VendorCapture, GossipCapture, QuestCapture, LootCapture, CombatCapture, PhaseTracker, EmoteCapture, CoverageHeatmap, DeltaHints). The new pause nil-return path is safe with no caller audit needed.

### 3.5 Correct pcall pattern
```lua
local ok, val = pcall(fn, arg)
result = ok and val or fallback
```
Safe for all our uses (returns are strings/numbers/tables, never boolean false). **Caveat**: This pattern is unsafe if `val` can be `false` — the `or` branch evaluates even on success. For any future API that returns a boolean, use the explicit form: `if ok then result = val else result = fallback end`.

### 3.6 SavedVariables size estimation (persistent, cumulative)

- `ESTIMATED_BYTES_PER_RECORD` constant (default 250, configurable via `config.capture.estimatedBytesPerRecord`).
- `totalFlushedRecords` is **persistent across sessions**. It is seeded from actual chunk data on `ADDON_LOADED` (for "VoxSniffer") via `FM.SeedFromDB()`, which sums `chunk.count` across all chunks in `VoxSnifferDB.chunks`. This fires on both fresh login AND `/reload`. New flushes add to this counter. It is NEVER reset between sessions.
- Estimated SV size = `totalFlushedRecords * config.capture.estimatedBytesPerRecord`.
- Budget thresholds: 80% warn (one-time log per pause cycle via `warnLogged` flag), 95% auto-pause.
- **Resume**: `/vs resume` clears `paused` flag but does NOT affect the counter. If still near 95%, the next flush re-pauses. User must increase `svBudgetMB` or purge data.
- **Session start**: `SM.Start()` calls `FM.ClearPause()` to clear the paused state. Counter remains at its cumulative value.
- **Budget check skipped for exports**: When `FM.FlushAll("export")` is called, the budget check is skipped to prevent auto-pause during intentional data export.
- Python pipeline reports actual file size as ground-truth calibration.
- Guard: `FM.GetBudgetPercent()` uses `math.max(budgetBytes, 1)`.
- Note: `VoxSnifferDB.stats.total_observations` is a lifetime counter that includes pruned records. `PruneChunks` is the existing V1 function in `FlushManager.lua` that removes chunks older than a configured threshold — it deletes entire chunks from `VoxSnifferDB.chunks` but does not decrement `total_observations`. The budget uses actual chunk counts (via `SeedFromDB()`), not this stat.
- **SeedFromDB + PruneChunks interaction**: After PruneChunks removes old chunks, `SeedFromDB()` correctly reflects the SMALLER SV file. This is correct — the budget should match actual file contents. Pruned data is gone from the file.
- **Python calibration**: `os.path.getsize(sv_path)` reports actual SV file size. Implementer can add this to `status` output as a one-liner ground-truth check.
- **Naming**: Envelope top-level uses `map` (V1 backward compat). `LocationContext.Get()` returns `mapId` (descriptive). Asymmetry is intentional — Python `get_location()` handles both.
- **Consolidated diagnostics fields** (written to `session.diagnostics`):
  - `svEstimatedBytes` — FlushManager, written each flush (Phase 6)
  - `flushCount` — FlushManager, incremented each flush (Phase 6)
  - `plateChurnCount` — UnitScanner, written via FLUSH_COMPLETE EventBus callback (Phase 5)

### 3.7 Item link format validation gate
ItemLinkParser returns `{ rawLink = link, itemId = parsedId }` only until validated in-game (Step 9).

### 3.8 Python parsing is already safe
`lua_table_parser.py` uses recursive regex, not eval/exec.

### 3.9 Export safety
In-game exports generate text for copy-paste. VoxSniffer NEVER writes to external addon SavedVariables.

### 3.10 Schema v2 migration strategy
**Detection**: Field-presence on individual envelopes. If `lc` field present -> v2. Both coexist in VoxSnifferDB. `SCHEMA_VERSION` bumped to 2 in both Constants.lua and Python config.py.

### 3.11 Module aliases
`Cfg` = `NS.Config`, `SM` = `NS.SessionManager`, `FM` = `NS.FlushManager`, `EB` = `NS.EventBus` (local aliases per file). `NS.modules` = registry table (defined near top of VoxSniffer.lua). `args` = parsed from slash input. `Log` = `NS.Log` (used in FlushManager).

### 3.12 Payload location contract (addon-to-Python)

**Rule**: V2 modules REMOVE inline position-gathering code. V2 envelopes carry location exclusively in top-level fields (`map`, `pos`, `lc`). Payload fields `mapId`/`position`/`playerPosition` are NOT present in v2 envelopes. Module-specific position data (e.g., ObjectTracker's `vignettePosition`) IS kept in payload.

**Python normalizers**: Updated to use a shared `get_location(record)` helper defined in `normalize/utils.py` (the sole definition — see Phase 7 for the implementation). Returns `(mapId, pos)` reading from `lc` envelope fields (v2) or falling back to payload fields (v1). All normalizers import it via `from normalize.utils import get_location`.

### 3.13 Pause notification contract

When `FM.IsPaused()` causes `MakeEnvelope()` to return nil, the first dropped observation triggers a one-time warning log via `FM.NotifyPauseDrop()`. The `pauseNotified` flag is cleared by `FM.ClearPause()`, so a new warning fires if capture re-pauses.

### 3.14 EventBus (existing V1 component)

`Core/EventBus.lua` already exists in V1 (loaded via `VoxSniffer.toc` before `VoxSniffer.lua`). It provides a simple publish-subscribe system within the addon namespace:
- `NS.EventBus.Subscribe(eventName, callback)` — registers a callback for a named event.
- `NS.EventBus.Publish(eventName, data)` — fires all registered callbacks for the event synchronously (inline, same frame, subscription order). There is no deferred/async execution. The `data` argument is passed by reference to all callbacks.

V2 uses EventBus for one event: `"FLUSH_COMPLETE"`, published by `FM.FlushAll()` (Phase 6) and subscribed to by UnitScanner (Phase 5) for writing `plateChurnCount` diagnostics. Modules may use `local EB = NS.EventBus` as a local alias.

### 3.15 warnLogged re-fire behavior

After the user runs `/vs resume` at a fill level above 80% (e.g., 92%), the `warnLogged` flag is cleared by `FM.ClearPause()`. On the next flush, the 80% warning fires again. This is intentional — the user should be re-warned after manually resuming into a high-fill state. If the fill reaches 95%, the system auto-pauses again.

### 3.16 Scheduler behavior during loading screens

The WoW `OnUpdate` timer (which drives `NS.Scheduler`) pauses during loading screens — no ticks fire. `LC.Refresh()` is also called from `ZONE_CHANGED_NEW_AREA`, which fires after load completes. Between these two callers, the LocationContext cache is always refreshed promptly after any zone transition.

## 4) API Availability on TrinityCore 12.x (build 66263)

**Confirmed working** (already called in shipped V1):
`C_Map.GetBestMapForUnit`, `C_Map.GetPlayerMapPosition`, `GetRealZoneText`, `GetSubZoneText`, `UnitGUID`, `GetMerchantNumItems`/`GetMerchantItemInfo`, `GetNumLootItems`/`GetLootSlotInfo`/`GetLootSlotLink`, `UnitPosition`, `NAME_PLATE_UNIT_ADDED/REMOVED`, `select(2, GetBuildInfo())`, `GetLocale`, item link match `"item:(%d+)"`.

**Needs validation**: `GetInstanceInfo`, `C_Map.GetMapInfo(mapId)`, `C_CVar.GetCVar("nameplateMaxDistance")`, `GetNumAddOns`/`GetAddOnInfo(i)`/`IsAddOnLoaded(i)`, item link fields beyond itemId, `BasicFrameTemplateWithInset` `TitleText` child availability.

## 5) V1 Under-Capture Gaps

Subzone text (`GetSubZoneText`), instance metadata (`GetInstanceInfo`), item bonus IDs (link fields), scan environment (CVARs, addon list).

## 6) File Structure

```text
tools/publishable/VoxSniffer/
+-- VoxSniffer.toc                   # Updated: add LocationContext.lua + ItemLinkParser.lua
+-- VoxSniffer.lua                   # Modified: remove private cache, MakeEnvelope uses LC,
|                                    #   add /vs inspect, /vs export, /vs resume,
|                                    #   split init: ADDON_LOADED + PLAYER_LOGIN
+-- Core/
|   +-- Constants.lua                # Modified: SCHEMA_VERSION 2, ESTIMATED_BYTES_PER_RECORD = 250
|   +-- Config.lua                   # Modified: add debug.externalLogSink, capture.svBudgetMB,
|   |                                #   capture.estimatedBytesPerRecord
|   +-- Logging.lua                  # Modified: optional DLAPI sink
|   +-- Scheduler.lua                # Unchanged
|   +-- EventBus.lua                 # EXISTING V1 (Section 3.14): Subscribe/Publish, synchronous
|   +-- SavedVariablesSchema.lua     # Modified: schema_version 2, v1->v2 migration
|   +-- SessionManager.lua           # Modified: FM.ClearPause() in Start(), scan environment
|   +-- FlushManager.lua             # Modified: SeedFromDB, budget checks, pause/resume, NotifyPauseDrop
|   +-- LocationContext.lua          # NEW
|   +-- ItemLinkParser.lua           # NEW
+-- Modules/
|   +-- UnitScanner.lua              # Modified: generation counter, ResetState, churn diagnostic
|   +-- GossipCapture.lua            # Modified: remove inline player-position
|   +-- VendorCapture.lua            # Modified: remove inline position + item parsing, use ILP
|   +-- QuestCapture.lua             # Modified: remove inline position + item parsing, use ILP
|   +-- LootCapture.lua              # Modified: remove inline item parsing, use ILP
|   +-- ObjectTracker.lua            # Modified: remove mouseover position + vignette playerPosition
|   +-- MovementTracker.lua          # UNCHANGED (Section 3.3)
|   +-- (others unchanged)
+-- UI/
|   +-- ControlPanel.lua             # Modified: add SV size estimate display

tools/voxsniffer/                    # PYTHON PIPELINE
+-- normalize/
|   +-- __init__.py                  # Modified: import LocationNormalizer, call enrich() in normalize_all()
|   +-- utils.py                     # NEW: get_location() shared helper
|   +-- location_normalizer.py       # NEW: instance tagging, coverage report
|   +-- vendor_normalizer.py         # Modified: use get_location() from utils
|   +-- gossip_normalizer.py         # Modified: use get_location() from utils
+-- exporters/
|   +-- __init__.py                  # Modified: add GM2Exporter
|   +-- gm2_exporter.py             # NEW: file-based classification, GM2 Lua output
+-- data/
|   +-- gameobject_types.csv         # NEW (optional): pre-exported from world.gameobject_template
+-- parsers/savedvariables_loader.py # Modified: v2 field-presence detection
+-- config.py                        # Modified: SCHEMA_VERSION=2, GM2_DIR, GAMEOBJECT_TYPES_CSV
+-- cli.py                           # Modified: gm2-export subcommand, --coverage on status
+-- tests/                           # Modified: v2 fixtures + coexistence tests
```

## 7) Implementation Phases

### Phase 1 -- LocationContext + MakeEnvelope + TOC + stubs

**VoxSniffer.toc**: Insert after `Core\FlushManager.lua`:
```
Core\LocationContext.lua
Core\ItemLinkParser.lua
```

**Core/LocationContext.lua** (new):
```lua
local _, NS = ...
NS.LocationContext = {}
local LC = NS.LocationContext

local cached = {
    mapId = 0, pos = nil, zone = "",
    subZone = nil, instanceName = nil, instanceType = nil,
    difficultyID = nil, difficultyName = nil,
}

function LC.Refresh()
    local ok0, mapId = pcall(C_Map.GetBestMapForUnit, "player")
    mapId = ok0 and mapId or 0
    if mapId == 0 then return end  -- keep previous valid cache; Scheduler retries in 1s

    cached.mapId = mapId
    cached.pos = nil
    local okp, p = pcall(C_Map.GetPlayerMapPosition, mapId, "player")
    if okp and p then cached.pos = { x = p.x, y = p.y } end

    local okz, zone = pcall(GetRealZoneText)
    cached.zone = (okz and zone) or ""

    local ok1, sz = pcall(GetSubZoneText)
    cached.subZone = (ok1 and sz and sz ~= "") and sz or nil

    -- GetInstanceInfo returns 8+ values (name, instanceType, difficultyID, difficultyName, ...);
    -- pcall prepends the success bool, so ok2 captures success, then the first 4 return values follow
    if GetInstanceInfo then
        local ok2, name, iType, diffId, diffName = pcall(GetInstanceInfo)
        -- If pcall fails, all vars are nil; ternary assignments produce nil (no else needed)
        local isInstance = ok2 and (iType and iType ~= "none")
        cached.instanceName = isInstance and name or nil
        cached.instanceType = isInstance and iType or nil  -- nil for open world
        cached.difficultyID = isInstance and diffId or nil
        cached.difficultyName = isInstance and diffName or nil
    end
end

function LC.Get()
    return {
        mapId = cached.mapId,
        zone = cached.zone,
        pos = cached.pos and { x = cached.pos.x, y = cached.pos.y } or nil,
        subZone = cached.subZone,
        instanceName = cached.instanceName,
        instanceType = cached.instanceType,
        difficultyID = cached.difficultyID,
        difficultyName = cached.difficultyName,
    }
end

-- Load-time registration: Scheduler.lua is loaded before LocationContext.lua in TOC.
-- Core services register at load time; modules register in Enable/Disable.
NS.Scheduler.Register("LocationContext", LC.Refresh, 1.0)
```

**FlushManager.lua stubs** — split into TWO insertion points to satisfy Lua lexical scoping:

**Part A: Local state variables** — insert at the TOP of FlushManager.lua, AFTER the existing local declarations (after V1 line 14: `local totalFlushed = 0`), BEFORE `FM.RegisterBuffer` (V1 line 17). This ensures these locals are visible to `FM.FlushAll` (defined at V1 line 35) and all functions below:
```lua
-- V2 budget/pause state (Phase 1 stubs — Phase 6 does NOT change these declarations)
local totalFlushedRecords = 0
local paused = false
local pauseNotified = false
local warnLogged = false
```

**Part B: Function stubs** — add after the existing `FM.Reset()` function at the end of FlushManager.lua. Phase 6 REPLACES this function block only (the Part A locals above remain unchanged). `Log` alias is already at file scope (V1 line 9):
```lua
-- Phase 1 function stubs — Phase 6 REPLACES these functions (Part A locals stay)
function FM.IsPaused() return paused end
function FM.ClearPause()
    paused = false
    pauseNotified = false
    warnLogged = false
end
function FM.Resume()
    FM.ClearPause()  -- same behavior; Resume is the user-facing name
end
function FM.SeedFromDB()
    local db = VoxSnifferDB
    if not db or not db.chunks then
        totalFlushedRecords = 0
        return
    end
    local count = 0
    for _, chunk in pairs(db.chunks) do
        count = count + (tonumber(chunk.count) or 0)
    end
    totalFlushedRecords = count
end
function FM.NotifyPauseDrop()
    if pauseNotified then return end
    pauseNotified = true
    Log.Warn("Core", "Capture PAUSED -- observations are being dropped. Use /vs resume to continue.")
end
function FM.GetEstimatedBytes() return 0 end
function FM.GetBudgetPercent() return 0 end
```

**SessionManager.lua**: At top of `SM.Start()`, before the existing `if activeSession then` guard:
```lua
FM.ClearPause()  -- clear any prior SV budget pause
```
(Note: `NS.ResetAllModuleState()` is already called by all callers AFTER SM.Start() succeeds — slash:232, panel:143, compartment:345, auto:412. No change needed.)

**VoxSniffer.lua changes**:
- Remove: private cache vars (`cachedMapId`, `cachedPos`, `cachedZone`), `RefreshPlayerContext()`, `contextTimer`, the 1s timer block in OnUpdate
- Replace ZONE_CHANGED_NEW_AREA handler call: `NS.LocationContext.Refresh()`
- **Split initialization** between two events:
  - **ADDON_LOADED** (new handler): fires on both fresh login AND `/reload`. When `addonName == "VoxSniffer"`:
    ```lua
    VoxSnifferDB = Schema.Validate(VoxSnifferDB)
    Cfg.Init(VoxSnifferDB.config)  -- pass .config sub-table, NOT entire DB
    FM.SeedFromDB()
    ```
  - **PLAYER_LOGIN** (existing handler): fires only on fresh login (NOT `/reload`). Remove Schema.Validate and Cfg.Init from here. Keep: module enable, auto-start, `NS.LocationContext.Refresh()`
- Register for `ADDON_LOADED` event (add alongside existing `PLAYER_LOGIN`, `PLAYER_LOGOUT`, `ZONE_CHANGED_NEW_AREA`)
- Modify `MakeEnvelope()`:

```lua
function NS.MakeEnvelope(obsType, entityKey, payload, extra)
    if not SM.IsActive() then return nil end
    if FM.IsPaused() then
        FM.NotifyPauseDrop()
        return nil
    end

    local lc = NS.LocationContext.Get()  -- copy #1 (lc.pos is a fresh sub-table)
    -- copy #2: independent from lc.pos (truthiness on lc.pos table, not lc.pos.x number)
    local topPos = lc.pos and { x = lc.pos.x, y = lc.pos.y } or nil
    return {
        t = obsType,
        ek = entityKey,
        sid = SM.GetId(),
        map = lc.mapId,
        zone = lc.zone,
        pos = topPos,
        lc = lc,
        ts = GetTime(),
        epoch = time(),
        src = extra and extra.source_module or nil,
        fp = extra and extra.fingerprint or nil,
        p = payload,
    }
end
```

**Inline position removal** (6 blocks across 5 modules):

| Module | Lines | Remove |
|--------|-------|--------|
| VendorCapture.lua | 46-50 | `C_Map.GetBestMapForUnit("player")` + `GetPlayerMapPosition` block |
| LootCapture.lua | ~69 | Same pattern |
| ObjectTracker.lua | 51-55 | Same pattern (mouseover path). Also remove `mapId` and `position` from payload (line 83-84) |
| ObjectTracker.lua | 118-123 | Same pattern (vignette path). Also remove `playerPosition` from payload (line 146). Keep `vignettePosition` |
| GossipCapture.lua | 110-116 | Same pattern. Remove `mapId`/`position` from payload |
| QuestCapture.lua | 199-205 | Same pattern. Remove `mapId`/`position` from payload |

**After refactor, module payload shapes**:
- ObjectTracker mouseover: `{goEntry, name, guid, tooltipLines}` (no mapId/position)
- ObjectTracker vignette: `{vignetteGUID, name, vignetteID, objectGUID, type, isDead, onWorldMap, onMinimap, isUnique, inFogOfWar, atlasName, hasTooltip, vignettePosition, entityKey, npcId, goEntry}` (no mapId/playerPosition)
- VendorCapture/GossipCapture/QuestCapture: payload location fields removed

### Phase 2 -- ItemLinkParser (T1.3)

**Core/ItemLinkParser.lua** (new):
```lua
local _, NS = ...
NS.ItemLinkParser = {}
local ILP = NS.ItemLinkParser

function ILP.Parse(itemLink)
    if not itemLink or itemLink == "" then return nil end
    local itemId = tonumber(itemLink:match("item:(%d+)"))
    if not itemId then return nil end
    return { rawLink = itemLink, itemId = itemId }
end
```

Wire into: VendorCapture.lua, LootCapture.lua, QuestCapture.lua (lines 48, 70, 190).

### Phase 3 -- Debug integrations + slash commands (T1.4, T1.5, T2.3 in-game)

**Logging.lua** DLAPI sink at end of `Output()`:
```lua
if NS.Config and NS.Config.Get then
    local cfg = NS.Config.Get()
    if cfg.debugMode and cfg.debug and cfg.debug.externalLogSink and _G["DLAPI"] then
        DLAPI.DebugLog("VoxSniffer", level .. "~" .. (module or "Core") .. "~" .. msg)
    end
end
```

**VoxSniffer.lua slash router** -- add after existing commands:

```lua
elseif cmd == "inspect" then
    if not _G["DevTool"] then
        print(C.COLOR .. "[VoxSniffer]" .. C.CLOSE .. " DevTool addon not installed.")
    else
        local target = args[2] or "session"
        if target == "session" then
            DevTool:AddData(SM.GetSession(), "VoxSniffer Session")
        elseif target == "config" then
            DevTool:AddData(Cfg.Get(), "VoxSniffer Config")
        elseif target == "modules" then
            DevTool:AddData(NS.modules, "VoxSniffer Modules")
        else
            print(C.COLOR .. "[VoxSniffer]" .. C.CLOSE .. " Usage: /vs inspect [session|config|modules]")
        end
    end

elseif cmd == "resume" then
    if FM.IsPaused() then
        FM.Resume()
        print(C.COLOR .. "[VoxSniffer]" .. C.CLOSE .. " Capture resumed (byte counter preserved).")
    else
        print(C.COLOR .. "[VoxSniffer]" .. C.CLOSE .. " Capture is not paused.")
    end

elseif cmd == "export" then
    local exportType = args[2]  -- NOT "format" (would shadow string.format)
    if exportType == "objects" then
        -- Flush pending buffers first (budget check skipped for "export" reason)
        -- Note: iterates all chunks in a single frame. For expected data volumes
        -- (<100K records) this completes well within the 6s script timeout.
        -- If datasets approach budget limits, consider chunked coroutine processing.
        FM.FlushAll("export")
        local db = VoxSnifferDB
        if not db or not db.chunks then
            print(C.COLOR .. "[VoxSniffer]" .. C.CLOSE .. " No data to export.")
        else
            local lines = {}
            for _, chunk in pairs(db.chunks) do
                -- C.MODULE.OBJECT_TRACKER = "ObjectTracker" (defined in Constants.lua)
                if chunk.module == C.MODULE.OBJECT_TRACKER then
                    for _, rec in ipairs(chunk.records or {}) do
                        if rec.p then
                            -- Position: use vignettePosition for vignettes (actual coords),
                            -- fall back to envelope pos for mouseover (player pos as proxy;
                            -- WoW provides no API for game object world coordinates)
                            local objPos = rec.p.vignettePosition or rec.pos
                            local entry = rec.p.goEntry or rec.p.vignetteID or 0
                            local name = rec.p.name or "?"
                            lines[#lines + 1] = format("%s,%s,%.4f,%.4f,%d",
                                tostring(entry), name,
                                objPos and objPos.x or 0, objPos and objPos.y or 0,
                                rec.map or 0)
                        end
                    end
                end
            end
            if #lines == 0 then
                print(C.COLOR .. "[VoxSniffer]" .. C.CLOSE .. " No object data found.")
            else
                NS.ShowExportFrame("VoxSniffer Object Export",
                    "entry,name,x,y,mapId\n" .. table.concat(lines, "\n"))
            end
        end
    else
        print(C.COLOR .. "[VoxSniffer]" .. C.CLOSE .. " Usage: /vs export objects")
    end
```

Update help text to include: `inspect | export | resume`

**NS.ShowExportFrame** (cached, reusable) — add in VoxSniffer.lua before the slash command handler so it's available when `/vs export` calls it:
```lua
local exportFrame = nil
function NS.ShowExportFrame(title, text)
    if not exportFrame then
        exportFrame = CreateFrame("Frame", "VoxSnifferExportFrame", UIParent, "BasicFrameTemplateWithInset")
        exportFrame:SetSize(500, 400)
        exportFrame:SetPoint("CENTER")
        exportFrame:SetFrameStrata("DIALOG")
        local eb = CreateFrame("EditBox", nil, exportFrame)
        eb:SetMultiLine(true)
        eb:SetFontObject(ChatFontNormal)
        eb:SetPoint("TOPLEFT", exportFrame, "TOPLEFT", 12, -30)
        eb:SetPoint("BOTTOMRIGHT", exportFrame, "BOTTOMRIGHT", -12, 12)
        eb:SetScript("OnEscapePressed", function() exportFrame:Hide() end)
        exportFrame.editBox = eb
        -- Guard against duplicate UISpecialFrames entries if ShowExportFrame called twice
        local found = false
        for _, v in ipairs(UISpecialFrames) do
            if v == "VoxSnifferExportFrame" then found = true; break end
        end
        if not found then tinsert(UISpecialFrames, "VoxSnifferExportFrame") end
    end
    exportFrame.TitleText:SetText(title)
    exportFrame.editBox:SetText(text)
    exportFrame.editBox:HighlightText()
    exportFrame:Show()
end
```

### Phase 4 -- Session environment recording (T1.6)

**SessionManager.lua** -- in `SM.Start()`, after `modules_enabled` block (after line 55):
```lua
local env = {}

local ok1, val1 = pcall(C_CVar.GetCVar, "nameplateMaxDistance")
env.nameplateMaxDistance = ok1 and val1 or "unavailable"

-- GetBuildInfo() returns (version, build, date, tocversion, ...).
-- select(N, ...) returns all values from position N onward; assigning to one var captures only the first.
env.clientBuild = select(2, GetBuildInfo()) or "0"
env.interfaceVersion = select(4, GetBuildInfo()) or 0  -- tocversion (client interface number)

env.addons = {}
if GetNumAddOns then
    local ok2, count = pcall(GetNumAddOns)
    if ok2 and count then
        for i = 1, count do
            -- GetAddOnInfo(i) returns (name, title, notes, loadable, reason, security, newVersion)
            -- We capture only name; loadable (position 4) is NOT the same as "enabled"
            local ok3, name = pcall(GetAddOnInfo, i)
            if ok3 and name then
                -- Use IsAddOnLoaded to check if addon is actually loaded
                local ok4, loaded = pcall(IsAddOnLoaded, i)
                if ok4 and loaded then
                    env.addons[#env.addons + 1] = name
                end
            end
        end
    end
end

env.enabledModules = {}
for name, enabled in pairs(cfg.modules) do
    if enabled then
        env.enabledModules[#env.enabledModules + 1] = name
    end
end

activeSession.environment = env
```

### Phase 5 -- UnitScanner lifecycle hardening (T2.2)

Add the following to UnitScanner.lua. The EventBus subscription (below) is inert until Step 10 completes Phase 6, which adds the `FLUSH_COMPLETE` publisher to `FM.FlushAll()`. This is harmless — subscribing before the publisher exists just means no events fire yet.

```lua
local EB = NS.EventBus  -- local alias for EventBus (existing V1 component, Section 3.14)
local nameplateGeneration = {}
local lastScannedGeneration = {}
local plateChurnCount = 0
```

On `NAME_PLATE_UNIT_ADDED`:
```lua
nameplateGeneration[token] = (nameplateGeneration[token] or 0) + 1
activeNameplates[token] = true
```

On `NAME_PLATE_UNIT_REMOVED`:
```lua
activeNameplates[token] = nil
plateChurnCount = plateChurnCount + 1
nameplateGeneration[token] = nil
lastScannedGeneration[token] = nil
```

On scan tick:
```lua
local gen = nameplateGeneration[token]
local lastGen = lastScannedGeneration[token]
-- nil = never scanned = always scan first time
if lastGen == nil or gen > lastGen then
    -- perform scan
    lastScannedGeneration[token] = gen
end
```

**Diagnostics write path** (wired via EventBus — Section 3.14):
```lua
-- Subscribe to FLUSH_COMPLETE event published by FM.FlushAll (Phase 6)
-- Callbacks are synchronous: plateChurnCount is written in the same frame as the flush
EB.Subscribe("FLUSH_COMPLETE", function(data)
    if not data or not data.session then return end
    data.session.diagnostics = data.session.diagnostics or {}
    data.session.diagnostics.plateChurnCount = plateChurnCount
end)
```

**State reset** — `scanner` IS the module table registered in `NS.modules["UnitScanner"]` (UnitScanner.lua line 14: `local scanner = {}`). When `NS.ResetAllModuleState()` iterates `NS.modules` and calls `mod.ResetState()`, it reaches this function directly:
```lua
function scanner.ResetState()
    wipe(nameplateGeneration)
    wipe(lastScannedGeneration)
    wipe(activeNameplates)
    plateChurnCount = 0
end
```

**Generation counter scope**: Detects mid-session token recycling (GUID swap on a token without REMOVED/ADDED events). After REMOVED cleanup, the next ADDED starts a fresh generation. Cross-cycle detection is handled by the ADDED event itself triggering a new scan.

### Phase 6 -- SV budget + schema (T2.4, T2.5)

**FlushManager.lua** full implementation. Replace the Phase 1 **Part B function stubs** only (the block after `FM.Reset()`). The **Part A local variables** (`totalFlushedRecords`, `paused`, `pauseNotified`, `warnLogged`) declared near the top of the file remain unchanged — do NOT re-declare them. The file-scope `local Log = NS.Log` (V1 line 9) also remains:

```lua
-- === V2 budget/pause functions (replaces Phase 1 Part B stubs) ===
-- Part A locals (totalFlushedRecords, paused, pauseNotified, warnLogged) are at file top — NOT re-declared here
-- Log alias is at file scope (V1 line 9) — NOT re-declared here

function FM.IsPaused() return paused end

function FM.ClearPause()
    paused = false
    pauseNotified = false
    warnLogged = false
end

function FM.Resume()
    FM.ClearPause()  -- unified implementation; Resume is the user-facing name
end

function FM.SeedFromDB()
    local db = VoxSnifferDB
    if not db or not db.chunks then
        totalFlushedRecords = 0
        return
    end
    local count = 0
    for _, chunk in pairs(db.chunks) do
        count = count + (tonumber(chunk.count) or 0)
    end
    totalFlushedRecords = count
end

function FM.NotifyPauseDrop()
    if pauseNotified then return end
    pauseNotified = true
    Log.Warn("Core", "Capture PAUSED -- observations are being dropped. Use /vs resume to continue.")
end

function FM.GetEstimatedBytes()
    local cfg = NS.Config.Get()
    local bpr = (cfg.capture and cfg.capture.estimatedBytesPerRecord) or C.ESTIMATED_BYTES_PER_RECORD
    return totalFlushedRecords * bpr
end

function FM.GetBudgetPercent()
    local cfg = NS.Config.Get()
    local budgetBytes = ((cfg.capture and cfg.capture.svBudgetMB) or 50) * 1024 * 1024
    budgetBytes = math.max(budgetBytes, 1)
    return FM.GetEstimatedBytes() / budgetBytes
end
-- === End V2 budget/pause block ===
```

In `FM.FlushAll(reason)`, insert after the existing per-flush-cycle accumulation. The V1 code has a local `totalFlushed` variable used for logging the flush result — `totalFlushed = totalFlushed + totalRecords`. Insert the V2 budget tracking immediately after that line. Note: `totalFlushed` (V1, per-flush logging) and `totalFlushedRecords` (V2, persistent budget counter) are separate variables:
```lua
totalFlushedRecords = totalFlushedRecords + totalRecords

-- Write diagnostics to session
local session = NS.SessionManager.GetSession()
if session then
    session.diagnostics = session.diagnostics or {}
    session.diagnostics.svEstimatedBytes = FM.GetEstimatedBytes()
    session.diagnostics.flushCount = (session.diagnostics.flushCount or 0) + 1
end

-- Notify modules via EventBus (UnitScanner writes plateChurnCount here)
NS.EventBus.Publish("FLUSH_COMPLETE", { session = session, totalRecords = totalRecords })

-- Budget check (skip for explicit exports)
if reason ~= "export" then
    local pct = FM.GetBudgetPercent()
    if pct >= 0.95 and not paused then
        paused = true
        Log.Error("Core", format("Capture PAUSED: SV estimate %.1fMB exceeds 95%% budget. /vs resume to continue.",
            FM.GetEstimatedBytes() / 1024 / 1024))
    elseif pct >= 0.80 and not paused and not warnLogged then
        warnLogged = true
        local budgetMB = (NS.Config.Get().capture and NS.Config.Get().capture.svBudgetMB) or 50
        Log.Warn("Core", format("SV approaching limit: ~%.1fMB / %dMB (%.0f%%)",
            FM.GetEstimatedBytes() / 1024 / 1024, budgetMB, pct * 100))
    end
end
```

**Constants.lua**: `C.ESTIMATED_BYTES_PER_RECORD = 250`

**Config.lua** DEFAULTS additions: `capture = { svBudgetMB = 50, estimatedBytesPerRecord = 250 }`, `debug = { externalLogSink = false }` (gates the DLAPI sink in Logging.lua)

**SavedVariablesSchema.lua**: `C.SCHEMA_VERSION = 2`. Add migration:
```lua
if fromVersion < 2 then
    -- v1->v2: no structural changes needed; new lc field is additive
    db.schema_version = 2
end
```

**`/vs status`**: Add SV estimate line:
```lua
local estMB = FM.GetEstimatedBytes() / 1024 / 1024
local budgetMB = (Cfg.Get().capture and Cfg.Get().capture.svBudgetMB) or 50
local pct = FM.GetBudgetPercent() * 100
print(format("  SV estimate: ~%.1fMB / %dMB (%.0f%%)", estMB, budgetMB, pct))
if FM.IsPaused() then
    print("  |cffff4444PAUSED: SV budget exceeded. /vs resume|r")
end
```

### Phase 7 -- Python pipeline

**config.py**:
```python
SCHEMA_VERSION = 2
GM2_DIR = DATA_DIR / "exports"
GAMEOBJECT_TYPES_CSV = Path(__file__).parent / "data" / "gameobject_types.csv"
```

**savedvariables_loader.py**: Field-presence detection:
```python
def detect_schema_version(record):
    """Per-envelope detection: lc field present = v2."""
    return 2 if "lc" in record else 1
```

**normalize/utils.py** (new) — the SOLE home for `get_location()`:
```python
def get_location(record):
    """Read location from envelope (v2) or payload (v1 fallback)."""
    lc = record.get("lc")
    if lc:
        return lc.get("mapId", record.get("map", 0)), lc.get("pos")
    p = record.get("p", {})
    return p.get("mapId", record.get("map", 0)), p.get("position")
```

**normalize/location_normalizer.py** (new) — imports `get_location` from utils, defines a standalone enricher (NOT a `BaseNormalizer` subclass, since it processes all record types regardless of `obs_type`):
```python
from normalize.utils import get_location


class LocationNormalizer:
    """Enrich all records with instance metadata from the lc envelope field.

    This is a cross-cutting enricher, not a domain-specific normalizer.
    It does NOT extend BaseNormalizer (no obs_type, no filter_records).
    Called as a pre-processing step in normalize_all() before domain normalizers.
    """

    def enrich(self, records):
        for rec in records:
            if not isinstance(rec, dict):
                continue
            lc = rec.get("lc")
            if not lc:
                continue
            # Tag instance data for downstream domain normalizers
            if lc.get("instanceName"):
                rec["instance_name"] = lc["instanceName"]
                rec["instance_type"] = lc.get("instanceType")
                rec["difficulty_id"] = lc.get("difficultyID")
                rec["difficulty_name"] = lc.get("difficultyName")
        return records
```
Optional enrichment from AreaTable CSV if available (path uses scraper build number, currently `wago/merged_csv/12.0.1.66337/enUS/AreaTable-enUS.csv` -- adjust if build differs).

**normalize/__init__.py**: Import `LocationNormalizer` from `location_normalizer`. Do NOT add it to `ALL_NORMALIZERS` (it lacks the `BaseNormalizer` interface — no `obs_type`, no `filter_records()`). Instead, call it as a pre-processing step in `normalize_all()`:
```python
from .location_normalizer import LocationNormalizer

def normalize_all(records: list[dict]) -> dict[str, dict]:
    """Run all normalizers and return results keyed by obs_type."""
    # Pre-process: enrich all records with location/instance metadata
    LocationNormalizer().enrich(records)
    results = {}
    for norm in ALL_NORMALIZERS:
        filtered = norm.filter_records(records)
        if filtered:
            results[norm.obs_type] = norm.normalize(records)
    return results
```

**vendor_normalizer.py**: Import `get_location` from `normalize.utils`. Replace `p.get("mapId")` / `p.get("position")` with `get_location(record)`.

**gossip_normalizer.py**: Same change.

**exporters/gm2_exporter.py** (new, file-based):
```python
import csv
import math
from pathlib import Path

def load_gameobject_types(csv_path):
    """Load optional gameobject_template types CSV."""
    types = {}
    if csv_path and Path(csv_path).exists():
        with open(csv_path) as f:
            for row in csv.DictReader(f):
                types[int(row["entry"])] = {"type": int(row["type"]), "name": row.get("name", "")}
    return types

# GM2 type mapping: gameobject_template.type -> GM2 node type
GO_TYPE_TO_GM2 = {2: "Mining", 3: "Herb", 25: "Fishing"}

def encode_coord(x, y):
    """GM2 coordinate encoding. Clamp to [0, 0.9999] to prevent overflow/negative corruption.
    Without clamping, coord=1.0 produces 100,010,000 (overflows). With clamping,
    max value is 99,999,999. Edge-of-map nodes shift inward by ~0.01%.
    """
    cx = math.floor(max(0, min(x, 0.9999)) * 10000 + 0.5)
    cy = math.floor(max(0, min(y, 0.9999)) * 10000 + 0.5)
    return cx * 10000 + cy
```

**GM2 target output format** (GatherMate2 SavedVariables structure):
```lua
GatherMate2DB = {
    ["Herb"] = { [mapId] = { [encodedCoord] = goEntry, ... }, ... },
    ["Mining"] = { ... },
    ["Fishing"] = { ... },
}
-- goEntry = gameobject_template.entry ID (the game object's template entry)
-- encodedCoord = cx * 10000 + cy where cx/cy = floor(clamp(coord, 0, 0.9999) * 10000 + 0.5)
-- Without clamping, coord=1.0 produces 100,010,000 (overflows). Clamping to 0.9999 produces
-- 99,999,999 (max safe value). Edge-of-map nodes shift inward by ~0.01%.
```

If CSV absent, exports raw coordinates without classification (logs warning). One-time CSV prerequisite:
```sql
SELECT entry, type, name FROM world.gameobject_template WHERE type IN (2, 3, 25)
INTO OUTFILE '/path/to/gameobject_types.csv' FIELDS TERMINATED BY ',' ENCLOSED BY '"' LINES TERMINATED BY '\n';
```
**Security warning**: `INTO OUTFILE` requires the MySQL `FILE` global privilege and writes to the server's filesystem. Use a secure, non-web-accessible path. Configure `--secure-file-priv` appropriately. Revoke FILE privilege after use. Alternative: `mysql -e "SELECT ..." > file.csv` from command line (no FILE privilege needed).

**Object payload shapes in Python**: The GM2 exporter handles both mouseover and vignette payloads:
- Mouseover: `p.goEntry` for classification, position from envelope `pos` (player proxy)
- Vignette: `p.goEntry` (if available via `objectGUID`), position from `p.vignettePosition` (actual coords, preferred) or envelope `pos` (fallback)

**exporters/__init__.py**: Add `GM2Exporter`.

**cli.py**:
- Add `gm2-export` subcommand: `python -m voxsniffer gm2-export --input <SV> [--types-csv <CSV>] [--output <path>]`
- Add `--coverage` flag on `status` subcommand. Output to stdout:
```
Coverage Report:
  Observation types: unit_seen (45,231), vendor_snapshot (89), ...
  Unique zones: 12 (mapIds: 2222, 2244, ...)
  Sessions: 5 | Date range: 2026-03-01 to 2026-03-13
  Modules with 0 observations: CombatEnricher, DeltaHints
```

**Tests**: Update `test_savedvariables_loader.py` with v2 envelope test cases. Update `sample_savedvariables.lua` with v2 sample data containing `lc` fields alongside v1 records.

### Phase 8 -- ControlPanel SV display

**UI/ControlPanel.lua**: Add SV estimate to stats. Show pause warning if `FM.IsPaused()`.

### Phase 9 -- Item link enrichment (T2.1) -- BLOCKED

Gate: In-game validation of item link format. See Step 9.

## 8) Constraints

- Zero external addon dependencies for core capture
- All new API calls wrapped in pcall (Section 3.5)
- LocationContext uses Scheduler-driven 1s refresh with mapId=0 resilience (Section 3.2)
- LocationContext.Get() returns new table per call, no aliasing (Section 3.4)
- MovementTracker untouched (Section 3.3)
- SV budget counter is persistent, seeded from DB chunks on load (Section 3.6)
- Pause cleared on session start via SM.Start() -> FM.ClearPause() (Section 3.6)
- Pause drop notified once via FM.NotifyPauseDrop() (Section 3.13)
- Schema v2 uses field-presence detection (Section 3.10)
- Payload location contract: v2 modules remove inline position; Python uses get_location() (Section 3.12)
- Python test fixtures updated for v1/v2 coexistence

## 9) Acceptance Criteria

1. `Core/LocationContext.lua` registers 1s Scheduler callback at load time. `Get()` returns independent copy, `Refresh()` updates cache. mapId=0 returns early, preserving previous valid data.
2. VoxSniffer.lua private cache removed; MakeEnvelope reads from LocationContext with separate pos copies (no aliasing). Pause check calls `FM.NotifyPauseDrop()` on first drop.
3. Inline player-position code removed from VendorCapture, LootCapture, ObjectTracker (both paths), GossipCapture, QuestCapture (6 blocks). Payload `mapId`/`position`/`playerPosition` fields removed from these modules.
4. MovementTracker's NPC position code untouched.
5. `Core/ItemLinkParser.lua` with itemId extraction; wired into VendorCapture, LootCapture, QuestCapture.
6. `/vs inspect [session|config|modules]` works with DevTool, warns when absent.
7. DLAPI sink in Logging.lua, gated behind config + global check.
8. Session environment: nameplateMaxDistance, clientBuild, interfaceVersion, addon names (via IsAddOnLoaded), enabledModules.
9. UnitScanner generation counter with cleanup on REMOVED. `scanner.ResetState()` wipes all tables (exposed via `NS.modules["UnitScanner"]`). `plateChurnCount` written via `FLUSH_COMPLETE` EventBus subscription.
10. FlushManager: `SeedFromDB()` counts actual chunk records on `ADDON_LOADED` (fires on both login and `/reload`). Budget warns at 80% (once per cycle), pauses at 95%. Budget check skipped for "export" reason. Resume clears pause only.
11. Schema v2 in SavedVariablesSchema.lua (with v1->v2 migration) and Python config.py.
12. Python: `get_location(record)` helper in normalize/utils.py. vendor/gossip normalizers import from utils.
13. `/vs export objects` uses `exportType` (not `format`), correct position source (`vignettePosition` or envelope `pos`), `C.MODULE.OBJECT_TRACKER` (defined in Constants.lua).
14. Python `gm2_exporter.py` with file-based CSV classification (optional). CLI: `gm2-export` subcommand.
15. `/vs resume` clears pause without resetting counter. `FM.NotifyPauseDrop()` fires once per pause cycle.
16. All start paths clear pause via `FM.ClearPause()` in SM.Start().
17. Python test fixtures updated for v2 envelope coexistence.
18. VoxSniffer.toc updated with LocationContext.lua and ItemLinkParser.lua.
19. All new API calls use correct pcall pattern.
20. `FM.GetBudgetPercent()` guards against division by zero.
21. `ShowExportFrame` cached and reusable, named `VoxSnifferExportFrame`, EditBox anchored with inset offsets.
22. `--coverage` flag on `status` subcommand reports obs types, unique zones, session count, date range, modules with 0 observations.
23. `instanceType` set to nil for open world (consistent with other instance fields).

## 10) Implementation Order

Steps map to Phases in Section 7 as noted below.

1. **(Phase 1)** VoxSniffer.toc update (add LocationContext.lua + ItemLinkParser.lua after FlushManager.lua)
2. **(Phase 1)** Core/LocationContext.lua + FlushManager stubs (IsPaused, ClearPause, Resume, SeedFromDB, NotifyPauseDrop, with `local Log = NS.Log` alias) + SessionManager FM.ClearPause() + VoxSniffer.lua refactor (remove private cache, split init: ADDON_LOADED for Schema.Validate/Cfg.Init/FM.SeedFromDB, PLAYER_LOGIN for module enable/auto-start/LC.Refresh, update MakeEnvelope)
3. **(Phase 1)** Remove inline player-position from 5 modules (6 blocks including ObjectTracker vignette). Remove payload location fields.
4. **(Phase 2)** Core/ItemLinkParser.lua + wire into VendorCapture, LootCapture, QuestCapture
5. **(Phase 6)** SavedVariablesSchema.lua v2 + Config.lua + Constants.lua
6. **(Phase 3)** Logging.lua DLAPI sink + slash router commands (inspect, resume, export) + ShowExportFrame + help text
7. **(Phase 4)** SessionManager scan environment (addon names via IsAddOnLoaded, CVARs, build)
8. **(Phase 5)** UnitScanner generation counter + ResetState + FLUSH_COMPLETE EventBus subscription + churn diagnostics. **Note**: The EventBus subscription is inert until Step 10 completes Phase 6 (which adds the publisher). This is harmless — subscribing before the publisher exists just means no events fire yet.
9. **GATE**: In-game test session -- validate item link format. Capture 10+ real links.
10. **(Phase 6)** FlushManager full implementation (replaces stubs): SeedFromDB, budget checks, diagnostics, NotifyPauseDrop, FLUSH_COMPLETE EventBus publish
11. **(Phase 8)** ControlPanel SV display
12. **(Phase 7)** Python: config.py v2 + savedvariables_loader v2 + normalize/utils.py + location_normalizer + normalize/__init__.py + vendor/gossip normalizer updates
13. **(Phase 7)** Python: gm2_exporter + exporters/__init__.py + data/gameobject_types.csv placeholder
14. **(Phase 7)** Python: cli.py gm2-export subcommand + --coverage on status
15. **(Phase 7)** Python: test fixture updates (v2 envelope samples + v1/v2 coexistence tests)
16. **(Phase 9)** T2.1 ItemLinkParser enrichment (after Step 9 gate)
