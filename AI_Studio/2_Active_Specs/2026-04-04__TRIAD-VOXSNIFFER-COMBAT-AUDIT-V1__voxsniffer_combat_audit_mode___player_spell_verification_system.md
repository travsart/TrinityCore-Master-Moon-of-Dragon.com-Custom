---
spec_id: TRIAD-VOXSNIFFER-COMBAT-AUDIT-V1
title: VoxSniffer Combat Audit Mode — Player Spell Verification System
status: Approved for Implementation
priority: P0
date: 2026-04-04
architect: ChatGPT
systems_architect_qaqc: Antigravity
intended_implementer: Claude Code
workflow: VoxCore Triad
---

# VoxSniffer Combat Audit Mode — Player Spell Verification System

## 1) Goal & Scope
Implement a new opt-in VoxSniffer module named CombatAudit that captures player-centric combat telemetry for offline spell verification, integrates with the existing envelope/ring-buffer/SavedVariables pipeline, exposes audit slash commands, and adds a Python audit report generator under tools/voxsniffer/. This stream must deliver v1 only: player-focused CLEU capture, talent/spec snapshot, aura timeline capture, periodic spell statistics aggregation, session lifecycle management, compact audit summary, and a data-driven proc expectation framework with basic timeout tracking sufficient for configured class relationships. Out of scope for this spec: retail-data cross-referencing, definitive automated balance/scaling judgments, generalized per-class expert systems, full passive-talent semantic inference, UI panels, persistent settings UX, replacing CombatCapture, or modifying server/C++ logic. Resource anomaly heuristics may be scaffolded but are not required for acceptance unless directly needed to support audit_summary counters.

## 2) Problem Statement
Current VoxSniffer combat telemetry is intentionally biased toward NPC-centric analysis and excludes pure player spell interactions. As a result, when a player reports that a class spell, proc, talent, aura, or modifier feels broken, there is no authoritative player-perspective event stream to validate what actually occurred in combat. Server logs and TrinityCore data only reveal intended mechanics, not the in-client observable outcome. CombatAudit closes that gap by recording the player’s own combat actions, expected proc chains, talents, auras, and summarized outcomes into structured SavedVariables that can be converted into a Markdown diagnostic report for Claude Code-driven spell verification and bug isolation.

## 3) Architectural Decisions
### 3.1 CombatAudit is a standalone module, not a mode switch inside CombatCapture
CombatCapture is purpose-built for NPC-oriented collection and currently encodes an NPC participant filter. Reusing that module for player audit mode would entangle two materially different collection intents, increase regression risk for shipped VoxSniffer behavior, and make opt-in diagnostics harder to reason about. A dedicated module keeps player verification concerns isolated while still reusing shared core infrastructure.

**Approved Behavior:**
Create `addons/VoxSniffer/Modules/CombatAudit.lua` as an independently registered module with its own enable/disable lifecycle, internal state, and event routing. CombatCapture remains unchanged except for any optional shared dispatch hook that is strictly additive and backward compatible.

**Disallowed Behavior:**
Do not repurpose CombatCapture into a dual-mode collector. Do not remove or weaken the existing NPC-only filtering behavior in CombatCapture. Do not merge audit-only data into CombatCapture observation types.

### 3.2 Audit mode is explicitly opt-in and session-scoped
Player-combat telemetry is high-volume and diagnostic in nature. It should not be always-on because that would inflate SavedVariables, add unnecessary CPU cost during routine gameplay, and blur the distinction between baseline sniffing and targeted spell audits. A session-scoped mode also aligns with the requested slash-command workflow.

**Approved Behavior:**
Default config must set `modules.combatAudit.enabled = false`. Actual event capture begins only after `/vox audit start` and stops after `/vox audit stop`. The module may be loaded while inactive, but it must not emit player combat observations outside an active audit session.

**Disallowed Behavior:**
Do not enable player audit capture by default. Do not record continuous player combat data just because the addon is loaded.

### 3.3 Use a single normalized internal event ingestion path inside CombatAudit
Combat audit requires multiple outputs from one CLEU event: raw player_combat observations, aura timeline updates, proc expectation tracking, and spell statistics aggregation. Centralizing parsing/normalization prevents drift between sub-features and avoids duplicated subevent decoding logic.

**Approved Behavior:**
Implement one internal entrypoint such as `CombatAudit:HandleCombatLogEvent(...)` that parses CLEU once, derives normalized payload fields, then fans out to helper methods like `ProcessPlayerCombatObservation`, `ProcessAuraEvent`, `ProcessProcExpectations`, and `ProcessSpellStatistics`.

**Disallowed Behavior:**
Do not parse CLEU argument shapes separately in multiple helper functions in inconsistent ways. Do not maintain divergent field mappings for the same subevent.

### 3.4 Player participation filter is inclusive and based on source OR destination GUID equality to player GUID
The audit must capture offensive, defensive, self-buff, proc, aura, and incoming interaction evidence. Restricting to source-only or destination-only would miss major classes of spell verification data.

**Approved Behavior:**
A CLEU event is eligible when `sourceGUID == UnitGUID("player")` OR `destGUID == UnitGUID("player")`. Self-cast, pet-adjacent, and target-facing events are preserved if the player is one side of the interaction. Store source/dest flags so downstream analysis can distinguish directionality.

**Disallowed Behavior:**
Do not reintroduce creature/vehicle filters. Do not discard self-only buff/proc events. Do not require an NPC participant.

### 3.5 Proc expectations are data-driven and class-loaded from static Lua tables
The intake packet explicitly rejects hardcoded per-spell logic embedded in capture code. A class-scoped data file makes the system maintainable, auditable, and extensible as more classes/specs are added.

**Approved Behavior:**
Add `addons/VoxSniffer/Data/ProcExpectations.lua` exposing a table keyed by class token and optionally specialization ID. Load only the current player class branch on audit start. Each expectation entry must minimally contain `triggerSpellId`, `expectedProcSpellId`, `expectedChance`, `windowSeconds`; optional metadata such as `triggerSubEvents`, `minOccurrencesBeforeTimeout`, `notes`, or `auraSpellId` is allowed.

**Disallowed Behavior:**
Do not hardcode Warlock proc rules directly in CombatAudit.lua. Do not require SavedVariables configuration editing for baseline functionality.

### 3.6 Proc timeout logic is watchdog-based but conservative in v1
The request wants expected procs and missing-proc detection, but robust statistical truth for probabilistic mechanics is complex. v1 should surface suspicious absence patterns without overclaiming certainty. Conservative thresholds reduce false positives.

**Approved Behavior:**
When a trigger event matches a configured expectation, record a pending expectation window with trigger timestamp and counters. Mark it fulfilled if the expected proc spell/aura appears within the configured window. Emit `proc_timeout` only when configured criteria are met, including `minOccurrencesBeforeTimeout` defaulting to a conservative class-safe value if omitted. Confidence language in downstream reports must remain suggestive, not definitive.

**Disallowed Behavior:**
Do not declare a proc broken after a single missed probabilistic trigger unless an expectation explicitly configures deterministic behavior. Do not present probabilistic misses as guaranteed defects.

### 3.7 Spell statistics are accumulator-based with periodic flushes, not emitted per damage event
Per-hit summary writes would duplicate data already available in raw player_combat observations and would put unnecessary pressure on the ring buffer and SavedVariables size. The intake packet explicitly requests accumulator-based periodic flush.

**Approved Behavior:**
Maintain in-memory per-spell accumulators for cast/hit/crit/total/min/max and flush `spell_stats` observations every 30 seconds during active sessions and once again on audit stop. Include player stat snapshot fields available at flush time such as spell power, attack power, mastery effect, haste percent, versatility bonus if API-accessible.

**Disallowed Behavior:**
Do not emit `spell_stats` on every CLEU line. Do not compute rolling stats by rescanning historical observations each flush.

### 3.8 Aura tracking uses lifecycle state keyed by aura spell plus target role
To measure applied→removed durations, refresh counts, and stacks, the module needs an in-memory state map across CLEU subevents. Pure event logging is insufficient for timeline summaries.

**Approved Behavior:**
Track aura state in-memory for any aura where the player is source or destination and especially for player self-buffs/debuffs. On apply/refresh/remove/dose events, update state keyed at minimum by `destGUID + spellId + auraType`. Emit a completed `aura_timeline` observation on remove or session stop for open auras. Preserve `appliedAt`, `removedAt`, `duration`, `expectedDuration` when obtainable, `maxStacks`, and `refreshCount`.

**Disallowed Behavior:**
Do not rely solely on downstream Python reconstruction for aura durations if the addon can compute them at source. Do not drop open auras at session end without emitting a best-effort timeline.

### 3.9 Talent snapshot is captured once at audit start and stored as authoritative session metadata
Talent effects can only be interpreted if the player loadout at test time is known. Capturing this once on session start creates a stable reference and avoids partial drift from later changes. It also keeps v1 implementation tractable.

**Approved Behavior:**
On `/vox audit start`, capture specialization, specialization name, class token, active talents, hero talents if API-accessible on 12.x, and available baseline stat context. Emit one `talent_snapshot` observation immediately. Reference this snapshot in session state for later summary/report generation.

**Disallowed Behavior:**
Do not defer talent snapshot until combat begins. Do not omit spec/talent data when the audit starts successfully.

### 3.10 Slash-command integration must extend the existing command surface without breaking current commands
The requested UX is command-driven. Commands should map directly to module lifecycle functions and produce deterministic chat feedback. Existing VoxSniffer command behavior must remain stable.

**Approved Behavior:**
Extend the addon command router to support `/vox audit start`, `/vox audit stop`, `/vox audit status`, and `/vox audit report`. Each command must be idempotent where sensible and print concise user-facing status to DEFAULT_CHAT_FRAME.

**Disallowed Behavior:**
Do not add a separate slash root. Do not require reloads between start and stop. Do not overload unrelated existing commands with audit semantics.

### 3.11 Python report generator is a separate consumer, not part of addon runtime
Lua-side capture should remain lightweight and focused on collecting canonical observations. Heavier interpretation belongs in the offline Python toolchain where Claude Code consumes results.

**Approved Behavior:**
Add `tools/voxsniffer/audit_report.py` and supporting parser changes so Lua writes only structured observations and Python performs filtering, aggregation, and Markdown rendering. The report must prominently call out suspicious spells, proc timeouts, talent anomalies if present, and key session metadata.

**Disallowed Behavior:**
Do not attempt to generate Markdown reports inside the addon. Do not place Python-only interpretation logic in SavedVariables schema code.

### 3.12 Backward compatibility with existing SavedVariables envelope schema is mandatory
VoxSniffer already ships with a chunk/envelope pipeline, and the intake packet explicitly requires reuse. Altering the storage contract would risk breaking existing tools and modules.

**Approved Behavior:**
CombatAudit must emit observations through the existing ring buffer and envelope utilities. New observation `t` values are additive only. Existing top-level VoxSnifferDB schema keys and flush mechanics must remain unchanged.

**Disallowed Behavior:**
Do not create a parallel SavedVariables file. Do not mutate historical observation shapes unrelated to CombatAudit.

## 4) File Structure
```text
addons/
└── VoxSniffer/
    ├── VoxSniffer.toc                        # add new data/module files in load order
    ├── VoxSniffer.lua                       # bootstrap + slash command routing integration
    ├── Core/
    │   ├── Config.lua                       # add modules.combatAudit defaults
    │   └── SavedVariablesSchema.lua         # document/add new observation types if schema table exists
    ├── Data/
    │   └── ProcExpectations.lua             # NEW class/spec keyed proc expectation table
    └── Modules/
        └── CombatAudit.lua                  # NEW player-focused audit collector

tools/
└── voxsniffer/
    ├── __init__.py                          # if present, unchanged or minimal export update
    ├── parser.py                            # extend observation recognition if required by current parser design
    ├── audit_report.py                      # NEW Markdown report generator
    └── tests/
        ├── test_audit_report.py             # NEW Python report coverage
        └── fixtures/
            └── combat_audit_sample.lua      # NEW or JSON fixture representing SavedVariables observations

doc/
└── (optional, only if project convention requires)
    └── voxsniffer_combat_audit.md           # implementation notes / usage, only if repo already documents addon modules

```

## 5) Logic & Data Flow
1. Addon load: VoxSniffer core loads Config, core utilities, ProcExpectations data, and the CombatAudit module in TOC order. CombatAudit registers itself via the existing `RegisterModule` pattern but remains inactive by default.
2. Command start: User runs `/vox audit start`. The command router calls `CombatAudit:StartSession()`.
3. Session initialization: `StartSession()` verifies no session is active, captures `UnitGUID("player")`, player name, class token, realm if available, current specialization via `GetSpecialization()`/`GetSpecializationInfo()`, talents/hero talents via available talent APIs, initializes in-memory session state, loads class/spec proc expectations from `Data/ProcExpectations.lua`, resets spell accumulator maps, aura state maps, pending proc watchdogs, counters, and start timestamp.
4. Snapshot emission: CombatAudit enqueues a `talent_snapshot` observation using the existing ring buffer/envelope pipeline. It may also enqueue an optional lightweight `audit_session_started` internal marker if the existing schema conventions support that, but this is not required.
5. Event subscription: During an active session, CombatAudit listens for `COMBAT_LOG_EVENT_UNFILTERED`. If the addon architecture already centralizes frame dispatch, register there; otherwise CombatAudit owns its own event frame. Each CLEU callback retrieves the current combat log payload once.
6. Player participation filter: The module compares sourceGUID/destGUID to the session player GUID. If neither matches, the event is ignored. No NPC/creature requirement applies.
7. Event normalization: For matched events, `HandleCombatLogEvent` maps the CLEU payload into a normalized Lua table including timestamp, subEvent, sourceGUID/name/flags, destGUID/name/flags, spellId/spellName/spellSchool where applicable, aura type where applicable, amount/overkill/school/resisted/blocked/absorbed/critical/glancing/crushing/offHand/environmental fields depending on subevent shape.
8. Raw observation write: CombatAudit emits a `player_combat` observation containing the normalized event payload. Supported subevents include the existing CombatCapture coverage plus the explicitly required additions: `SPELL_EXTRA_ATTACKS`, `DAMAGE_SHIELD`, `SPELL_ABSORBED`, `SPELL_DRAIN`, `SPELL_LEECH`, `SPELL_INSTAKILL`, `PARTY_KILL`.
9. Aura processing: If the subevent is one of `SPELL_AURA_APPLIED`, `SPELL_AURA_REFRESH`, `SPELL_AURA_REMOVED`, `SPELL_AURA_APPLIED_DOSE`, `SPELL_AURA_REMOVED_DOSE`, `SPELL_AURA_BROKEN`, or `SPELL_AURA_BROKEN_SPELL`, CombatAudit updates an in-memory aura record keyed by destination GUID and spell ID. It tracks first application time, last refresh time, current stacks, max stacks observed, refresh count, source relationship, and expected duration if derivable from API or left nil. On removal or session end, it emits an `aura_timeline` observation.
10. Proc expectation triggering: For any normalized event whose subevent and spellId match an expectation entry for the current class/spec, CombatAudit creates or updates a pending watchdog record. Record fields should include triggerSpellId, expectedSpellId, expectedChance, firstSeenAt, lastSeenAt, triggerCount, windowSeconds, and expectation metadata.
11. Proc fulfillment: If a later event matches the expected proc spell (or expected aura, if metadata allows), CombatAudit resolves the oldest compatible pending watchdog, emits `proc_fulfilled` with latency in milliseconds and trigger/proc IDs, increments session counters, and removes or decrements pending state.
12. Proc timeout scanning: On each relevant CLEU event and also on a lightweight periodic OnUpdate or timer tick while a session is active, CombatAudit scans pending watchdogs. If `now - windowStart >= windowSeconds` and threshold criteria such as `triggerCount >= minOccurrencesBeforeTimeout` are satisfied without fulfillment, it emits `proc_timeout` containing triggerSpellId, expectedSpellId, tickCount/triggerCount, windowElapsed, expectedChance, and notes/confidence metadata, then closes that watchdog.
13. Spell statistics accumulation: For cast, damage, heal, miss, and aura/proc events as appropriate, CombatAudit updates per-spell accumulators in memory. Recommended accumulator fields: `castCount`, `successCount`, `hitCount`, `critCount`, `missCount`, `totalDamage`, `totalHealing`, `minHit`, `maxHit`, `lastCastAt`, and `sampleCount`. These maps are keyed by spellId.
14. Periodic stats flush: Every 30 seconds during an active session, CombatAudit gathers current player stat context using available WoW API (spell bonus damage/healing, attack power, crit chance, haste, mastery, versatility if exposed) and emits one `spell_stats` observation per non-empty accumulator. After flush, accumulators may either reset for interval-based reporting or continue cumulatively; for v1 choose cumulative-with-`intervalSeconds` metadata OR reset-per-interval consistently. Preferred behavior: cumulative since session start, with `windowStart` and `windowEnd` fields.
15. Optional anomaly scaffolding: If simple deterministic talent expectations are implemented in v1 for a small set of clearly observable cases, CombatAudit may emit `talent_anomaly` observations when a configured effect is absent. However, this is optional unless a deterministic class rule is easy to support. The Python report must tolerate zero such observations.
16. Status command: `/vox audit status` reads in-memory session counters and prints active/inactive state, elapsed duration, total raw events captured, proc expectations opened/fulfilled/timed out, aura records tracked, and number of spell stat entries currently accumulated.
17. Report command: `/vox audit report` prints a compact in-game summary from current in-memory state if the session is active or from the last finalized summary if it has stopped. This is a human quick-glance, not the canonical offline report.
18. Command stop: User runs `/vox audit stop`. CombatAudit finalizes all open aura timelines, performs a final proc timeout scan, flushes final `spell_stats`, computes session totals, emits `audit_summary`, tears down session-active flags, and prints completion status.
19. SavedVariables persistence: All CombatAudit observations ride through the existing ring buffer and flush manager into VoxSnifferDB envelopes/chunks with no schema break.
20. Python offline consumption: `tools/voxsniffer/audit_report.py` reads the SavedVariables export via the existing parser, filters observations of types `talent_snapshot`, `player_combat`, `proc_timeout`, `proc_fulfilled`, `spell_stats`, `aura_timeline`, `talent_anomaly`, and `audit_summary`, groups them by audit session if multiple are present, and renders Markdown.
21. Markdown output: The report includes session metadata, top suspicious spells by timeout/anomaly/low sample notes, proc chains that timed out, fulfilled procs with latency distributions if available, spell statistics tables, aura duration findings, and a concise ‘Likely Broken / Needs More Samples / No Issue Observed’ section using conservative wording.
22. Claude Code workflow: The generated Markdown becomes the canonical artifact Claude Code reads alongside server data to decide whether a spell, proc, aura, or talent likely fails on the TrinityCore server.

## 6) Constraints for Implementation
- Do not modify CombatCapture’s existing NPC-only semantics except for optional additive shared-dispatch wiring that preserves identical outputs.
- Default `modules.combatAudit.enabled` must be `false` in `addons/VoxSniffer/Core/Config.lua`.
- No player-combat observations may be captured unless an audit session is explicitly active via `/vox audit start`.
- All new observations must flow through the existing ring buffer, envelope, and flush pipeline; no parallel persistence path is allowed.
- Must support the explicitly missing CLEU subevents from the intake packet: `SPELL_EXTRA_ATTACKS`, `DAMAGE_SHIELD`, `SPELL_ABSORBED`, `SPELL_DRAIN`, `SPELL_LEECH`, `SPELL_INSTAKILL`, `PARTY_KILL`.
- Must preserve compatibility with WoW client build 12.0.1.66709 and TrinityCore private-server runtime assumptions described in the intake.
- Keep addon runtime lightweight: no per-event full-table scans of historical observations, no expensive string formatting in the hot path, and no chat spam from event capture.
- Proc expectation definitions must live in a dedicated data file, not inline as scattered conditionals inside CombatAudit.
- v1 must use conservative suspicion language for probabilistic proc failures; it must not claim certainty from low-sample randomness.
- Do not build a DPS meter, rankings UI, or generalized combat analytics dashboard as part of this stream.
- Do not attempt retail data cross-referencing, external web lookups, or server-side fixes in this spec.
- Slash commands must be idempotent and user-safe: starting an active session should not corrupt state; stopping an inactive session should print a clean status message.
- If hero talent APIs are unavailable or partial on this client build, snapshot what is accessible and store nil/empty metadata rather than failing session start.
- Spell stats must be accumulator-based and flushed periodically plus on session stop; per-hit `spell_stats` writes are prohibited.
- The Python tool must produce Markdown output suitable for Claude Code consumption and must not require addon runtime dependencies.
- Any optional talent anomaly detection in v1 must be deterministic and data-backed; avoid speculative heuristics that generate noisy false positives.
- Session finalization must emit `audit_summary` exactly once per started session.
- Open aura records and pending proc watchdogs must be resolved or force-finalized on `/vox audit stop` so the offline report is not left with dangling state.

## 7) Acceptance Criteria
- `addons/VoxSniffer/Modules/CombatAudit.lua` exists, registers successfully with VoxSniffer, and remains inactive until commanded.
- `addons/VoxSniffer/Core/Config.lua` includes `modules.combatAudit.enabled = false` or equivalent nested default without changing unrelated defaults.
- `addons/VoxSniffer/Data/ProcExpectations.lua` exists and contains a data-driven expectation table keyed by class token, with at least one functional seed set for Warlock.
- `addons/VoxSniffer/VoxSniffer.toc` is updated so the new data/module files load in a valid order before module registration/use.
- The addon supports `/vox audit start`, `/vox audit stop`, `/vox audit status`, and `/vox audit report` through the existing slash-command entrypoint.
- Starting an audit session emits a `talent_snapshot` observation containing specialization metadata and a non-empty talents array when the client API exposes talents.
- During an active audit, CLEU events where the player is source or destination produce `player_combat` observations with normalized payloads and no NPC filter.
- The explicitly missing CLEU subevents (`SPELL_EXTRA_ATTACKS`, `DAMAGE_SHIELD`, `SPELL_ABSORBED`, `SPELL_DRAIN`, `SPELL_LEECH`, `SPELL_INSTAKILL`, `PARTY_KILL`) are captured into `player_combat` when they involve the player.
- Aura apply/refresh/remove flows result in `aura_timeline` observations with duration, refreshCount, and maxStacks fields populated when observable.
- Configured proc expectations create `proc_fulfilled` observations when the expected proc appears inside the window and `proc_timeout` observations when the watchdog expires under configured criteria.
- `spell_stats` observations are emitted periodically during active sessions and on stop, containing cast/hit/crit and damage/healing aggregates for touched spells.
- Stopping a session emits a single `audit_summary` observation containing duration, totalEvents, procsExpected, procsFulfilled, procsTimedOut, and anomaliesDetected counters.
- No `player_combat`, `proc_*`, `spell_stats`, `aura_timeline`, or `audit_summary` observations are emitted when no audit session is active.
- The new observations persist inside the existing VoxSnifferDB chunk/envelope structure without breaking existing parser expectations for unrelated data.
- `tools/voxsniffer/audit_report.py` exists and can read a SavedVariables export to generate a Markdown report summarizing suspicious spells, proc timeouts, spell stats, and session metadata.
- Python tests or fixture-driven validation cover at least: parsing combat audit observations, rendering proc timeout sections, and rendering spell stats/session summary sections.
- Implementation does not break existing VoxSniffer load/bootstrap behavior or existing CombatCapture module behavior.

## 8) Recommended Implementation Order
### Phase 1 — Core module scaffolding and load wiring
- Update `addons/VoxSniffer/VoxSniffer.toc` to include `Data/ProcExpectations.lua` before `Modules/CombatAudit.lua`.
- Add `modules.combatAudit.enabled = false` in `addons/VoxSniffer/Core/Config.lua`.
- Create `addons/VoxSniffer/Data/ProcExpectations.lua` with class/spec keyed table structure and Warlock seed entries.
- Create `addons/VoxSniffer/Modules/CombatAudit.lua` with module registration, inactive default state, session state structure, and no-op lifecycle placeholders.

### Phase 2 — Slash command and session lifecycle integration
- Inspect existing slash command parsing in `addons/VoxSniffer/VoxSniffer.lua` and add `audit start|stop|status|report` subcommand routing.
- Implement `CombatAudit:StartSession()`, `StopSession()`, `GetStatus()`, and `PrintReportSummary()` methods.
- Implement talent/spec/class snapshot collection and immediate `talent_snapshot` observation enqueue on session start.
- Ensure idempotent start/stop behavior and concise chat feedback strings.

### Phase 3 — CLEU ingestion and normalized player_combat capture
- Implement active-session `COMBAT_LOG_EVENT_UNFILTERED` subscription/dispatch in CombatAudit.
- Add normalized combat log parsing helpers covering spell, swing, range, environmental, aura, drain/leech/absorbed, and extra attack payload variations.
- Apply inclusive player GUID filter (`sourceGUID == playerGUID or destGUID == playerGUID`).
- Emit canonical `player_combat` observations for all relevant events, including the newly required subevents.

### Phase 4 — Aura lifecycle tracking and spell statistics
- Implement in-memory aura state map keyed by `destGUID + spellId + auraType` or equivalent stable composite key.
- Track aura apply/refresh/remove/dose state and emit `aura_timeline` on remove and session stop.
- Implement per-spell accumulator map for casts, hits, crits, misses, damage, healing, min/max, timestamps.
- Add periodic 30-second stats flush and final stop flush emitting `spell_stats` observations with player stat snapshot fields.

### Phase 5 — Proc expectation watchdog system
- Load class/spec expectations from `ProcExpectations.lua` at session start.
- Implement trigger matching by spellId and optional subevent filters.
- Implement pending watchdog storage, fulfillment matching, timeout scanning, and observation emission for `proc_fulfilled` and `proc_timeout`.
- Add counters for `procsExpected`, `procsFulfilled`, and `procsTimedOut` into session summary state.

### Phase 6 — Session finalization and summary output
- Force-close open aura records on session stop with best-effort `removedAt` and duration computation.
- Run final timeout scan for pending proc watchdogs.
- Emit a single `audit_summary` observation with required counters and suspectedBroken scaffolding if supported.
- Implement `/vox audit status` and `/vox audit report` chat rendering from in-memory/finalized summary data.

### Phase 7 — Python parser and Markdown report generator
- Inspect current `tools/voxsniffer/` parser architecture and extend observation recognition only where necessary for new `t` values.
- Create `tools/voxsniffer/audit_report.py` with CLI entrypoint accepting SavedVariables path and optional output file path.
- Render Markdown sections for session metadata, suspicious spells, proc timeouts, spell statistics, aura findings, and compact conclusion language.
- Keep wording conservative for probabilistic misses and clearly label low-sample findings.

### Phase 8 — Tests, fixture validation, and regression pass
- Add fixture(s) representing combat audit observations including at least talent snapshot, player combat entries, proc timeout, spell stats, aura timeline, and audit summary.
- Add Python tests for report generation and parser handling of new observation types.
- Perform manual addon sanity review to ensure no breakage to existing VoxSniffer bootstrap or CombatCapture behavior.
- Prepare implementation handoff notes for Gemini audit via the established Triad workflow after coding is complete.

## 9) Immediate Next Actions
- Open `addons/VoxSniffer/VoxSniffer.lua`, `addons/VoxSniffer/VoxSniffer.toc`, `addons/VoxSniffer/Core/Config.lua`, and `addons/VoxSniffer/Modules/CombatCapture.lua` to confirm exact registration and slash-command patterns before writing code.
- Create `addons/VoxSniffer/Data/ProcExpectations.lua` first so CombatAudit can be written against a stable expectation schema.
- Implement the CombatAudit session lifecycle and slash commands before touching CLEU parsing; verify the module can start/stop cleanly with no active capture.
- Then implement normalized CLEU capture with the player GUID filter and required subevents, followed by aura/stat/proc subsystems in that order.
- After Lua-side observation emission is stable, extend `tools/voxsniffer/` with `audit_report.py` and fixture-driven tests, then run the mandated Gemini audit through the project review pipeline.
