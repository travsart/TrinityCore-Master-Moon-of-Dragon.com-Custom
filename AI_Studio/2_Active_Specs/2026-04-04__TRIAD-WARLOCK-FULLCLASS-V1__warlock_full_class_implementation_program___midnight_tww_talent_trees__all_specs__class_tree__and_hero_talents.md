---
spec_id: TRIAD-WARLOCK-FULLCLASS-V1
title: Warlock Full Class Implementation Program — Midnight/TWW Talent Trees, All Specs, Class Tree, and Hero Talents
status: Approved for Implementation
priority: P0
date: 2026-04-04
architect: ChatGPT
systems_architect_qaqc: Antigravity
intended_implementer: Claude Code
workflow: VoxCore Triad
---

# Warlock Full Class Implementation Program — Midnight/TWW Talent Trees, All Specs, Class Tree, and Hero Talents

## 1) Goal & Scope
Produce and implement a canonical Warlock class systems framework for TrinityCore-based Midnight client build 66709 that supports the full Warlock class tree, Affliction, Demonology, Destruction, and associated Hero Talent trees (Diabolist, Hellcaller, Soul Harvester). This stream must establish the architecture, data collection pipeline, code layout, script registration strategy, proc/modifier framework, and verification harness required to implement every Warlock spell/talent interaction exhaustively. In scope: C++ server-side implementation architecture, DB2 extraction and mapping workflow, per-spec script partitioning, proc/resource/pet/cooldown frameworks, and generation of four downstream canonical spec documents (class tree + 3 specs, with hero talent coverage integrated by applicable specialization). Out of scope for this spec: hand-authoring the full retail mechanics table inline here for every single talent node; client UI edits; non-Warlock classes; PvP talent implementation unless they are baseline/shared in PvE spell data; cosmetic-only visuals without gameplay impact; speculative mechanics not validated against DB2/Wago/spell data; direct gameplay tuning beyond retail parity.

## 2) Problem Statement
The current Warlock implementation is effectively non-functional at class-system level: many talents are broken, proc chains are missing, pet interactions are unreliable, and resource/cooldown logic does not reflect modern Midnight/TWW talent trees. Testing piecemeal is wasteful because the dependency graph across talents, aura mods, replacement spells, and hero talent effects is dense. A complete architecture-first implementation is required so the implementer can systematically extract authoritative data, map every node, classify what TrinityCore can already support, identify what requires custom scripting, and land the work in a maintainable, auditable structure rather than a one-off patch pile.

## 3) Architectural Decisions
### 3.1 Adopt a two-stage delivery: canonical data dossier generation first, gameplay implementation second
The intake requests exact retail mechanics for every node, with spell IDs and handler types. That level of precision cannot be safely improvised from memory. Wago DB2/spell effect data is available and must be treated as the source of truth. Therefore the first mandatory stage is generating four canonical Warlock design dossiers from extracted data, then implementing against those dossiers.

**Approved Behavior:**
Create a reproducible extraction/mapping pipeline under tools/ to emit structured Warlock datasets and downstream markdown/json dossiers for Class, Affliction, Demonology, and Destruction, each including hero talent overlays where applicable. Only after dossier completion should C++ implementation begin for bulk work.

**Disallowed Behavior:**
Do not directly implement dozens of scripts from remembered retail behavior without first capturing authoritative spell/talent IDs, effects, and dependency mappings. Do not collapse all specs into a single ad hoc implementation patch with no generated reference artifacts.

### 3.2 Partition implementation by domain: shared class systems, spec modules, hero overlays, and pet/resource frameworks
Warlock has deep cross-cutting behavior: class-tree nodes affect all specs, while hero talents and spec-specific proc chains overlay on top. Clean partitioning reduces regression risk and makes future audits feasible.

**Approved Behavior:**
Use dedicated source files for shared class tree mechanics, one file pair per specialization, one file pair per hero talent family if behavior is non-trivial, and one shared support layer for pet/resource/replacement/proc helpers.

**Disallowed Behavior:**
Do not place all Warlock logic into a single monolithic file such as spell_warlock.cpp. Do not duplicate shared helper logic across spec files.

### 3.3 Prefer TrinityCore native spell data and effect hooks first; add custom scripts only where native handling is insufficient
Many effects may already be representable through SpellInfo, aura effects, spell_bonus_data, proc flags, spell_linked_spell, or existing TrinityCore generic systems. Custom code should target only interactions that require conditional logic, dynamic proc routing, pet bookkeeping, summon extension, replacement rules, or non-standard resource handling.

**Approved Behavior:**
For every node in the generated dossiers, classify implementation as one of: Native/DB-only, SpellScript, AuraScript, Proc handler, Unit/Pet event hook, or Hybrid. Document the chosen type in the dossier and comments adjacent to code.

**Disallowed Behavior:**
Do not default every talent to custom AuraScript or SpellScript when DB/native support suffices. Do not leave implementation type undocumented.

### 3.4 Make Hero Talents an overlay layer, not a separate replacement implementation of baseline specs
Hero talent mechanics modify or extend the core spec behavior. Treating them as overlays keeps the spec and code aligned with modern talent architecture and avoids divergent duplicate rotations.

**Approved Behavior:**
Implement Hero Talent code in files that attach to or augment shared/spec events through helper APIs and script registration. The downstream dossiers for each spec must include the applicable hero tree overlays and their dependency edges to baseline talents/spells.

**Disallowed Behavior:**
Do not fork Affliction/Demonology/Destruction into separate 'with hero' variants. Do not hardcode hero behavior in unrelated baseline scripts when a helper/overlay registration point exists.

### 3.5 Build an authoritative Warlock registry for spell/talent relationships
The request explicitly calls for every talent node, row/column, dependencies, replacement chains, proc chains, target caps, and priorities. These relationships are too large to manage manually in comments alone.

**Approved Behavior:**
Create generated machine-readable registry files (JSON or YAML) under generated/ or doc/generated/ that list talent nodes, spell IDs, spec ownership, hero ownership, linked spells, handler classification, and implementation status. Consume this registry during implementation and testing.

**Disallowed Behavior:**
Do not maintain critical relationship data only in scattered source comments or ephemeral notes.

### 3.6 Enforce retail-parity exactness through source validation gates
Numbers, durations, proc chances, and target caps must be exact. A validation pass comparing extracted DB2/effect values to implementation annotations is necessary to avoid drift.

**Approved Behavior:**
Add validation scripts/tests that flag dossiers with missing IDs, unresolved mechanics, unknown handler type, or implementation state mismatches. Gate completion on zero unresolved Tier A nodes and no unknown spell IDs for implemented nodes.

**Disallowed Behavior:**
Do not mark a spec complete if core rotational nodes still rely on TBD values or undocumented guessed behavior.

### 3.7 Use priority tiers to sequence implementation, but not to reduce final exhaustiveness
The intake asks for ranking by A/B/C. This is for sequencing, not scope reduction. Tier A ensures playable class parity quickly; Tier B and C complete the class.

**Approved Behavior:**
Tag every node with A/B/C and execute in waves: Tier A rotational/resource engine first, Tier B important passives next, Tier C utility/final edge cases after. Final completion requires all tiers addressed or explicitly documented as native/no-op under current branch.

**Disallowed Behavior:**
Do not treat Tier C as optional backlog outside this P0 stream unless formally split into a follow-on spec.

### 3.8 Create explicit pet-system helpers for Demonology and sacrifice/extension mechanics
Demonology and several class talents depend on summon lifecycle, active demon counts, demonic tyrant extensions, and grimoire/sacrifice edge cases. These are fragile if implemented with scattered pet lookups.

**Approved Behavior:**
Add a shared WarlockPetSystem helper layer that centralizes demon classification, temporary summon attribution, duration extension, summoned pet enumeration, and sacrifice state checks. Spec files must call this helper rather than duplicating summon traversal logic.

**Disallowed Behavior:**
Do not reimplement demon enumeration or tyrant extension rules independently in multiple scripts.

### 3.9 Verification must include deterministic proc/resource tests and in-game command-driven smoke coverage
Warlock bugs often hide in proc chains, shard generation, pet counts, and refresh windows. Unit/integration coverage plus in-game smoke macros are required to prevent silent breakage.

**Approved Behavior:**
Add automated tests where framework support exists and provide GM/in-game smoke scripts covering shard generation/spending, proc triggers, summon extension, replacement spells, and hero talent overlays.

**Disallowed Behavior:**
Do not rely solely on compilation success or manual tooltip checks as proof of correctness.

## 4) File Structure
```text
VoxCore/
├── src/
│   ├── server/
│   │   ├── scripts/
│   │   │   ├── Spells/
│   │   │   │   ├── classes/
│   │   │   │   │   └── warlock/
│   │   │   │   │       ├── spell_warlock_shared.h
│   │   │   │   │       ├── spell_warlock_shared.cpp
│   │   │   │   │       ├── spell_warlock_class_tree.cpp
│   │   │   │   │       ├── spell_warlock_affliction.cpp
│   │   │   │   │       ├── spell_warlock_demonology.cpp
│   │   │   │   │       ├── spell_warlock_destruction.cpp
│   │   │   │   │       ├── spell_warlock_hero_diabolist.cpp
│   │   │   │   │       ├── spell_warlock_hero_hellcaller.cpp
│   │   │   │   │       ├── spell_warlock_hero_soul_harvester.cpp
│   │   │   │   │       ├── warlock_pet_system.h
│   │   │   │   │       ├── warlock_pet_system.cpp
│   │   │   │   │       ├── warlock_proc_router.h
│   │   │   │   │       ├── warlock_proc_router.cpp
│   │   │   │   │       ├── warlock_spell_registry.h
│   │   │   │   │       └── warlock_script_loader.cpp
│   │   │   └── CMakeLists.txt
│   │   └── game/
│   │       ├── Spells/
│   │       │   ├── SpellMgr.cpp                  # only if registration/native support extensions are required
│   │       │   ├── SpellMgr.h                    # minimal surgical edits only if unavoidable
│   │       │   └── AuraEffect.cpp                # only if generic TC bugfix needed for Warlock parity
│   │       └── Entities/
│   │           └── Pet/                          # only if summon lifecycle hooks require generic fixes
├── sql/
│   ├── custom/
│   │   ├── world/
│   │   │   ├── 2026_04_04_00_warlock_spell_script_names.sql
│   │   │   ├── 2026_04_04_01_warlock_spell_linked_spell.sql
│   │   │   ├── 2026_04_04_02_warlock_spell_proc.sql
│   │   │   ├── 2026_04_04_03_warlock_spell_ranks_and_overrides.sql
│   │   │   └── 2026_04_04_04_warlock_misc_tuning_overrides.sql
│   │   └── hotfixes/
│   │       └── 2026_04_04_00_warlock_verified_rows.sql   # only if hotfix layer is used in project workflow
├── tools/
│   ├── warlock/
│   │   ├── extract_warlock_db2.py
│   │   ├── build_warlock_registry.py
│   │   ├── classify_warlock_handlers.py
│   │   ├── generate_warlock_spec_docs.py
│   │   ├── validate_warlock_spec_docs.py
│   │   └── warlock_constants.py
├── doc/
│   ├── classes/
│   │   └── warlock/
│   │       ├── README.md
│   │       ├── warlock_registry.schema.json
│   │       ├── generated/
│   │       │   ├── warlock_class_tree_spec.md
│   │       │   ├── warlock_affliction_spec.md
│   │       │   ├── warlock_demonology_spec.md
│   │       │   ├── warlock_destruction_spec.md
│   │       │   ├── warlock_registry.json
│   │       │   ├── warlock_registry.csv
│   │       │   ├── warlock_spell_dependencies.json
│   │       │   └── warlock_implementation_status.json
│   │       ├── tests/
│   │       │   ├── warlock_smoke_checklist.md
│   │       │   ├── affliction_rotation_cases.md
│   │       │   ├── demonology_pet_cases.md
│   │       │   └── destruction_proc_cases.md
│   │       └── source_notes/
│   │           ├── wago_db2_sources.md
│   │           └── tc_native_coverage_audit.md
└── tests/
    ├── warlock/
    │   ├── test_warlock_registry.py
    │   ├── test_warlock_spec_generation.py
    │   └── test_warlock_priority_coverage.py
    └── integration/
        └── warlock/                              # add only if project already supports scripted integration tests

```

## 5) Logic & Data Flow
1. Source acquisition phase: `tools/warlock/extract_warlock_db2.py` reads authoritative Warlock talent/spell data from available Wago DB2/spell effect exports and any local extraction tables already used by the project. It normalizes spell IDs, talent node IDs, spec associations, row/column coordinates, hero tree membership, spell effect rows, aura options, proc flags, target caps, charge/cooldown metadata, and replacement/override references.
2. Registry construction phase: `tools/warlock/build_warlock_registry.py` merges raw extracted data into a canonical Warlock registry. Each registry row represents a talent/spell/mechanic node and includes: name, spell ID, talent node ID, tree (`class`, `affliction`, `demonology`, `destruction`, `hero_diabolist`, `hero_hellcaller`, `hero_soul_harvester`), row/column, mechanic summary, dependencies, modified spells, proc sources, proc targets, resource effects, pet interactions, cooldown/charge behavior, handler classification, TrinityCore-native coverage assessment, and priority tier A/B/C.
3. Handler classification phase: `tools/warlock/classify_warlock_handlers.py` scans registry rows and assigns preliminary implementation types. Example categories: `DB_ONLY`, `NATIVE_AURA`, `SPELLSCRIPT`, `AURASCRIPT`, `PROC_EVENT`, `PET_LIFECYCLE`, `SUMMON_EXTENSION`, `RESOURCE_ENGINE`, `SPELL_REPLACEMENT`, `TARGET_CAP_CUSTOM`, `HYBRID`. Rows requiring manual review are emitted with `REVIEW_REQUIRED` status and must be resolved before implementation of affected Tier A nodes.
4. Spec generation phase: `tools/warlock/generate_warlock_spec_docs.py` renders four canonical markdown specs under `doc/classes/warlock/generated/`: class tree, affliction, demonology, destruction. Each document includes applicable hero overlays, per-node details, exact values from source data, dependencies, implementation type, and implementation status. These documents become the implementer's canonical checklist.
5. Validation phase: `tools/warlock/validate_warlock_spec_docs.py` ensures there are no missing spell IDs, no duplicate node ownership, no unresolved Tier A mechanics, no undocumented choice-node alternatives, and no generated docs out of sync with registry JSON. Failing validation blocks gameplay implementation for unresolved areas.
6. Shared runtime framework phase: `spell_warlock_shared.*`, `warlock_proc_router.*`, and `warlock_pet_system.*` provide common enums, helper predicates, proc dispatchers, summon classification, active demon enumeration, aura owner lookups, soul shard utility functions, and reusable script helpers. This layer should expose stable APIs such as `IsWarlockSpell(...)`, `HasWarlockTalent(...)`, `CountActiveDemonologyDemons(...)`, `ExtendEligibleDemonDurations(...)`, `TryTriggerNightfallLikeProc(...)`, and `ApplySpellReplacementState(...)`.
7. Class tree implementation phase: `spell_warlock_class_tree.cpp` implements all shared/class-tree mechanics and common spell modifications that affect multiple specs. This includes general curses, baseline survivability/utility, summon baselines, Grimoire of Sacrifice if class-tree placed, shared replacement logic, and shared proc hooks.
8. Spec implementation phase: each specialization file implements spec-owned mechanics only:
   - `spell_warlock_affliction.cpp`: DoT-driven proc engine, shard generation/spending interactions, debuff amplifiers, execute/refresh logic, target spread mechanics, and affliction hero overlay hook points.
   - `spell_warlock_demonology.cpp`: demonic core generation/spending, Wild Imp/Felguard/greater demon bookkeeping, summon burst windows, tyrant extension behavior, pet-triggered talent interactions, and demonology hero overlay hook points.
   - `spell_warlock_destruction.cpp`: ember/shard spender modifiers, Havoc duplication rules, Chaos Bolt/Incinerate/Conflagrate proc and charge systems, AoE cleave routing, infernal interactions, and destruction hero overlay hook points.
9. Hero overlay phase: `spell_warlock_hero_diabolist.cpp`, `spell_warlock_hero_hellcaller.cpp`, and `spell_warlock_hero_soul_harvester.cpp` attach to shared/spec hooks. They may register spell scripts directly for hero-owned spells, but modifications to baseline spell outcomes should be routed through helper interfaces rather than copied into each spec file.
10. SQL wiring phase: add `spell_script_names`, `spell_linked_spell`, proc-related rows, rank/override rows, and any project-specific hotfix/tuning rows required for script attachment or replacement behavior. SQL files must be deterministic, idempotent within project conventions, and reference only verified IDs from the registry.
11. Registration phase: `warlock_script_loader.cpp` centralizes `AddSC_warlock_*` registrations. `CMakeLists.txt` is updated to compile the new file set. If script registration can piggyback on existing build conventions, prefer that over engine modifications.
12. Automated verification phase: Python tests assert registry completeness, generated-doc consistency, and priority coverage. If server-side automated spell tests exist, add focused Warlock cases for Tier A mechanics. Otherwise, maintain structured smoke checklists and GM command scripts in `doc/classes/warlock/tests/`.
13. In-game verification phase: use a GM Warlock at appropriate level, grant all talents/hero combinations, and run deterministic smoke scenarios for each spec and hero overlay. Validate shard/resource math, replacement states, proc rates under controlled casts/ticks, pet extension behavior, cooldown resets, and target-cap/AoE duplication behavior.
14. Completion phase: only mark this stream complete when the registry shows every node classified and implemented or confirmed native/DB-driven, all Tier A/B/C items have status, generated docs are in sync, SQL is applied cleanly, build passes, and smoke coverage is documented with no unresolved class-breaking defects.

## 6) Constraints for Implementation
- Do not fabricate exact spell IDs, proc rates, durations, target caps, or talent positions from memory. All numbers and IDs must come from Wago DB2/spell data or equivalent authoritative source material available to the project.
- Do not skip the dossier-generation step. The four downstream Warlock spec documents are mandatory deliverables of this stream, not optional documentation.
- Do not implement PvP talents unless they are intrinsically required by baseline class data for normal operation and explicitly present in the validated registry.
- Do not alter client-side talent UI or attempt to patch the Midnight client as part of this server-side class implementation stream.
- Do not introduce broad engine changes to TrinityCore unless a generic engine defect is proven to block correct Warlock behavior. Prefer localized script-layer solutions first.
- Any unavoidable edits under `src/server/game/` must be minimal, narrowly scoped, documented in code comments with `WARLOCK:` prefix, and referenced in the generated spec docs' native coverage audit.
- All new Warlock C++ code must live under `src/server/scripts/Spells/classes/warlock/` unless a helper truly belongs in a generic shared engine location.
- Every implemented talent/spell interaction must be traceable back to an entry in `doc/classes/warlock/generated/warlock_registry.json`. No orphan scripts.
- Choice nodes must represent both options explicitly in the registry and generated docs, even if only one option is selected at runtime.
- Hero talents must be mapped to applicable specs exactly as defined by modern talent architecture; do not apply hero effects globally across all Warlock specs.
- Use priority tiers A/B/C strictly for sequencing. Final class completion still requires all tiers accounted for.
- Where TrinityCore already supports a mechanic natively, document that fact instead of wrapping it in redundant custom code.
- Pet and summon behaviors must go through shared helper APIs; do not duplicate summon tracking logic inside individual SpellScript/AuraScript classes.
- All SQL rows must reference verified spell IDs from the generated registry and must be separated by concern: script names, linked spells, proc rows, overrides, tuning.
- Do not close this effort with 'mostly complete' status. The intake explicitly requires exhaustive coverage.
- If a mechanic cannot be validated from available data, mark it `BLOCKED_SOURCE_VALIDATION` in the registry and generated docs; do not silently guess.
- Maintain compatibility with TrinityCore custom extensions (RoleplayCore) and do not break unrelated class systems or generic pet handling.
- Implementation comments for non-obvious logic must cite the corresponding registry node key or spell ID(s) for auditability.
- Before implementation begins, claim relevant files in `doc/session_state.md` per Central Brain protocol; after completion, update session state and wrap-up artifacts.

## 7) Acceptance Criteria
- A reproducible extraction pipeline exists under `tools/warlock/` and can generate a canonical Warlock registry from available Wago/DB2 source data.
- `doc/classes/warlock/generated/warlock_registry.json` exists and contains every Warlock class-tree, Affliction, Demonology, Destruction, and applicable Hero Talent node with spell IDs, row/column, dependencies, implementation type, and priority tier.
- Four generated markdown specs exist: class tree, Affliction, Demonology, Destruction. Each includes exhaustive node-level entries and hero talent overlays where applicable.
- Every registry node is classified as one of: native/DB-driven, custom scripted, hybrid, blocked by source validation, or intentionally not applicable. No silent omissions.
- All Tier A Warlock mechanics are implemented and validated in code with no unresolved `REVIEW_REQUIRED` or `BLOCKED_SOURCE_VALIDATION` status unless explicitly approved by a follow-up spec.
- Tier B and Tier C mechanics are also implemented or documented as native/no-op with verified rationale; final registry coverage is 100% accounted for.
- Shared helper infrastructure exists for Warlock proc routing, spell replacement state, soul shard/resource helpers, and pet/summon lifecycle bookkeeping.
- Source files are partitioned as specified and build successfully in the project's normal CMake workflow.
- SQL migrations are created for script registrations and supporting spell/proc/link rows and apply cleanly in project environments.
- Generated docs and registry validation tests pass locally with no mismatch between code status and documentation status.
- At least one deterministic smoke checklist exists for each specialization, including hero talent overlay scenarios and class-tree shared mechanics.
- Demonology-specific validation covers active demon counting, temporary summon attribution, Demonic Core generation/spending, and Demonic Tyrant extension behavior.
- Destruction-specific validation covers charge systems, spender interactions, Havoc duplication behavior, AoE target caps, and cooldown/proc interactions.
- Affliction-specific validation covers DoT tick proc chains, shard generation/spending, spread/refresh interactions, and debuff amplification mechanics.
- Class-tree validation covers shared curses, summons, sacrifice/shared pet interactions, survivability, and cross-spec replacement or modifier effects.
- No new broad engine regressions are introduced; any engine-layer changes are documented and justified in `tc_native_coverage_audit.md`.
- Session coordination files are updated per protocol, and this stream is ready for subsequent Gemini audit after implementation.

## 8) Recommended Implementation Order
### Phase 0 — Coordination and source inventory
- Claim the Warlock stream and relevant file paths in `doc/session_state.md`.
- Inventory existing TrinityCore Warlock scripts, spell SQL, and any project-specific RoleplayCore class hooks to establish baseline/native coverage.
- Identify the exact local source location for Wago DB2/spell effect data and document inputs in `doc/classes/warlock/source_notes/wago_db2_sources.md`.

### Phase 1 — Extraction and canonical registry generation
- Implement `tools/warlock/extract_warlock_db2.py` to collect Warlock talent tree, spell, effect, aura, proc, cooldown, charge, and replacement metadata.
- Implement `tools/warlock/build_warlock_registry.py` to normalize and merge data into `warlock_registry.json` and CSV output.
- Define `doc/classes/warlock/warlock_registry.schema.json` and validate generated registry structure.
- Populate ownership domains: class tree, affliction, demonology, destruction, hero_diabolist, hero_hellcaller, hero_soul_harvester.

### Phase 2 — Handler classification and dossier generation
- Implement `tools/warlock/classify_warlock_handlers.py` to assign native/custom/hybrid handler types and initial A/B/C priorities.
- Implement `tools/warlock/generate_warlock_spec_docs.py` to emit the four canonical markdown specs.
- Implement `tools/warlock/validate_warlock_spec_docs.py` and Python tests to fail on missing IDs, unresolved Tier A nodes, missing choice-node branches, or doc/registry drift.
- Author `doc/classes/warlock/source_notes/tc_native_coverage_audit.md` summarizing what TrinityCore already handles versus what requires code.

### Phase 3 — Shared runtime scaffolding
- Create `spell_warlock_shared.*` with common enums, constants, helper predicates, and talent/spell lookup utilities.
- Create `warlock_proc_router.*` to centralize proc dispatch and avoid duplicated event handling.
- Create `warlock_pet_system.*` to centralize demon classification, summon tracking, demon counting, sacrifice state, and tyrant-extension eligibility.
- Create `warlock_script_loader.cpp` and wire CMake registration.

### Phase 4 — Class tree implementation
- Implement all shared/class-tree mechanics in `spell_warlock_class_tree.cpp` based on the generated dossier.
- Add SQL script-name/link/proc rows needed for class-tree mechanics.
- Verify replacement chains and shared pet/curses/survivability behaviors in smoke checklist format.

### Phase 5 — Specialization Tier A implementation
- Implement Affliction Tier A rotational/resource/proc mechanics in `spell_warlock_affliction.cpp`.
- Implement Demonology Tier A summon/resource/proc mechanics in `spell_warlock_demonology.cpp`.
- Implement Destruction Tier A charge/spender/proc/duplication mechanics in `spell_warlock_destruction.cpp`.
- For each implemented node, update `warlock_implementation_status.json` and ensure corresponding script registration/SQL exists.

### Phase 6 — Hero talent overlay implementation
- Implement Diabolist hero mechanics in `spell_warlock_hero_diabolist.cpp` using overlay hooks into relevant specs.
- Implement Hellcaller hero mechanics in `spell_warlock_hero_hellcaller.cpp` using overlay hooks into relevant specs.
- Implement Soul Harvester hero mechanics in `spell_warlock_hero_soul_harvester.cpp` using overlay hooks into relevant specs.
- Verify hero talents do not duplicate baseline logic and only modify eligible specs/spells.

### Phase 7 — Specialization Tier B/C completion
- Complete all remaining important passive and utility interactions across Affliction, Demonology, and Destruction.
- Implement or validate target caps, edge-case proc exclusions, cooldown resets, charge restoration, and uncommon replacement states.
- Resolve all remaining `REVIEW_REQUIRED` nodes or formally mark blocked with source-validation notes if evidence is missing.

### Phase 8 — Verification, smoke coverage, and audit readiness
- Run registry/doc validation tests and confirm zero unresolved Tier A nodes.
- Execute in-game smoke checklists for each spec and hero combination; document outcomes under `doc/classes/warlock/tests/`.
- Prepare concise implementation summary and handoff for Gemini audit via the established Triad pipeline.
- Update `doc/session_state.md` and wrap-up artifacts with completed files, blockers, and follow-up items if any.

## 9) Immediate Next Actions
- Create/claim a Warlock implementation entry in `doc/session_state.md` before touching code.
- Audit current repository for existing Warlock-related scripts, SQL rows, and any generic pet/resource helpers that can be reused.
- Scaffold `tools/warlock/` and implement the extraction + registry pipeline first; do not start bulk C++ scripting before the registry exists.
- Generate an initial `warlock_registry.json` with all discoverable nodes and mark unknown fields explicitly rather than omitting them.
- Produce the four generated Warlock markdown dossiers and validate them for completeness before implementation begins.
- After dossiers are generated, implement shared helper layers (`spell_warlock_shared`, `warlock_proc_router`, `warlock_pet_system`) before any spec-specific scripts.
- Sequence gameplay implementation by Tier A first across class tree and all specs, then hero overlays, then Tier B/C closure.
- When implementation is complete, route the result through the Gemini audit step required by the Central Brain Triad process.
