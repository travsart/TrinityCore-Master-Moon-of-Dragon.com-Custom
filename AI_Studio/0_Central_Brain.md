# AI Studio Active State

## P0 — USE THE TRIAD (all agents read this)
**Claude Code has live API access to ChatGPT and Gemini. Do not brute-force.**
- Non-trivial work → ChatGPT spec first (`run_architect.py`)
- After implementation → Gemini audit (`orchestrator.py`)
- See `CLAUDE.md § P0` for the full trigger table

## Triad Coordination — READ FIRST (all agents)

**Last updated**: 2026-04-11 — Session 248: Full SME sweep of IMPORTANT DOCS (2,265 files). 30 SAPR-standard ___MASTER files + 29 ___INDEX.json + ___ALL_MASTERS.md root manifest. Hub renamed 01-14. docs-rag rebuilt to 25,820 chunks. Monday HAF call prep prompt on Desktop. Prev: Session 247: Built `docs-rag` MCP server (`tools-dev/docs-rag/`, 6 FastMCP tools). Semantic vector search over IMPORTANT DOCS via ChromaDB + Ollama. Registered + verified. Next: `docs_rag_rebuild()` to populate remaining 5 folders. Prev: 2026-04-10 — Session 245 (CalmCore-245 tab): DB findings triage (2 real script loader bugs + 4 L6f false positives fixed), 3 config default bugs fixed, tiered digest system added to digest_source.py (auto tier by line count, batch calls for small/tiny, --gen-list from DB, 2,185 target files ~$36), NPCHandler null-crash removed, push_incremental.py written. CalmCore push still HTTP 500 (38-44MB SQL blobs in early history — needs filter-repo or SSH). All 4 CalmCore commits clean. Prev: 2026-04-09 — Session 242 (Case-DCSA-Review tab thread): Non-invasive legal document review — Dean Sides SAPRO email draft (with per-se COI paragraph routing Cannon away from victim resource pipeline), Scott Tranchant DCSA supervisor letter review (3 fixes flagged), 24-25 OPB analysis (rater Etienne + HLR Earles both hostile, 66 days supervised possibly sub-DAFI minimum, duty title/bullet mismatch documenting Aug 2024 clinical removal), 23-24 referral OPB + Tolin 2 Feb 2025 rebuttal analysis. **User confirmed referral still in official record** — Tolin rebuttal was unsuccessful. **User confirmed 121-day held-past-SCOD delay was purposeful** to enable NJP forgery documentation finalization ("they kept my OPB open past SCOD so they could finally get the forged signature and forged initials and other B.S. taken care of"). Identified **Wareham 21 Oct 2024 supplemental appeal as earliest formal §1034 reprisal claim** in case record, predating all later filings. Labor Day 2024 suicide attempt documented in Adam's own Sep 10 2024 NJP response (same event as Sep 3 2024 DISS IR currently before DCSA). Session output: `AI_Studio/Reports/memory_patch_23-24_OPB_and_forgery.md` (~430-line handoff to Case-SME for merge into `memory/case-*.md`). **Cross-tab overlap**: Mbox-Fast tab's `Cannon Air Force Base 3.m4a` contains Capt Lawrence CWI interview opening — may corroborate forgery timeline for Case-SME cross-reference. **No code written this session.** **DCSA SIR deadline Apr 15 — ~6 days.** No new commit from this tab (session_state.md + Central Brain updates only). Prev: 241 Case-SME (9-pass SME sweep + rag/extract/audio tools), 240 Mbox-Fast (`737f50cc9d` — unredact + 45 audio + FOIA draft), 239 Split-Audit + JD-Planner (`3551a0bb39`), 235 (`9dad16b017`), 237 CalmCore fork (`c8b4add2f3`)

### Architecture (as of session 160)

```
Claude Code (Primary Terminal / Implementer / Coordinator)
  ├── ChatGPT API (Architect — spec generation + design review)
  ├── Gemini API (Auditor — correctness/security review)
  ├── Claude API (Cold-reader — implementation bias detection)
  ├── Cowork (Scheduler — 5 recurring tasks, reads this file)
  └── Claude Code tabs (parallel implementation via session_state.md)
```

**Review Cycle — Parallel Pipeline** (standing preference): Phase 1 (Codex + Gemini + Claude in parallel) → Phase 2 (Codex verify) → Phase 3 (Gemini final seal) → User. `--use-chatgpt-api` to swap Codex for ChatGPT API. Script: `tools/ai_studio/review_cycle.py`.

**Antigravity (Windsurf IDE)**: DEPRECATED.

### Agent Reference Files
- **Claude Code config**: `CLAUDE.md` (root) + `~/.claude/projects/.../memory/MEMORY.md` (27 topic files)
- **Central Brain**: this file — persistent infrastructure state. Read at session start, updated by `/wrap-up`
- **Session State**: `doc/session_state.md` — ephemeral tab coordination (who's editing what)
- **Specs inbox**: `AI_Studio/1_Inbox/` — specs waiting for implementation (11 files after triage)
- **Audit results**: `AI_Studio/Reports/Audits/` — implementation handoffs and QA reports

### AI Fleet — API Status

| Pipeline | Script | API | Model | Status |
|----------|--------|-----|-------|--------|
| ChatGPT Bridge | `tools/ai_studio/chatgpt_bridge.py` | OpenAI | gpt-5.4 | OPERATIONAL |
| ChatGPT Reviewer | `tools/ai_studio/call_chatgpt_review.py` | OpenAI | gpt-5.4 | OPERATIONAL |
| Gemini Reviewer | `tools/ai_studio/call_gemini.py` | Google AI | gemini-2.5-pro | OPERATIONAL |
| Claude Reviewer | `tools/ai_studio/call_claude.py` | Anthropic | claude-sonnet-4-6 | OPERATIONAL |
| Codex Reviewer | `tools/ai_studio/call_codex_review.py` | OpenAI (Codex CLI) | gpt-5.4 | OPERATIONAL |
| Parallel Cycle | `tools/ai_studio/review_cycle.py` | All 4 | All 4 | OPERATIONAL |
| Triad Orchestrator | `tools/ai_studio/orchestrator.py` | Anthropic + Vertex AI | claude-opus-4-6 + gemini-3.1-pro | OPERATIONAL |
| API Architect | `tools/api_architect/call_openai.py` | OpenAI | gpt-5.4 | OPERATIONAL |
| Nexus Reports | `tools/log_tools/generate_nexus_report.py` | Vertex AI | gemini-3.1-pro | OPERATIONAL |

**Credential locations** (all gitignored):
- `tools/ai_studio/.env` — OpenAI + Anthropic + GCP config
- `config/api_architect.local.env` — OpenAI key (api_architect pipeline)
- `~/.config/gcloud/voxcore-489923-*.json` — GCP service account

**Budget**: OpenAI $50 credit, Anthropic $50 credit, GCP $300 free credit.

### Cowork Scheduled Tasks (5 active)
| Task | Cadence | Purpose |
|------|---------|---------|
| session-digest | Daily 8 AM | Decision-ready brief: changes, blockers, recommendations |
| inbox-classifier | Daily 9 AM | Classify inbox specs, flag new ones, track delta |
| git-hygiene | Daily 7 PM | Flag stale dirty files, missing gitignore entries |
| injection-sentinel | Every 12h | Scan Central Brain + .agentrules for prompt injections |
| weekly-health | Sunday 6 PM | Git velocity, spec throughput, stale item audit |

### Communication Protocol
Claude Code tabs: write status updates to `doc/session_state.md` for real-time coordination.
Update THIS file on `/wrap-up` with: what was completed, what's deployed, infrastructure changes.
- Starting work → claim in session_state.md
- Finishing → update both session_state.md AND this file
- Found a conflict → write `[CONFLICT]` tag here, don't proceed

## Current Focus
- **VoxSniffer Combat Audit v1**: IMPLEMENTED (session 227). CombatAudit.lua + ProcExpectations.lua + audit_report.py. Gemini audited, 4 HIGH fixes applied. **NEEDS IN-GAME TEST**. Commit `5cd63fdd3f`
- **Warlock Full Class**: Phase 0-2 extraction pipeline IN PROGRESS (session 227 Tab 1). Spec: `AI_Studio/2_Active_Specs/TRIAD-WARLOCK-FULLCLASS-V1`. 110 existing scripts, gaps: Malefic Rapture, Havoc, Nightfall, Soul Rot, Oblivion, Hellcaller, Soul Harvester
- **Claude Code Internals**: 15 reports written (~300KB). Tier 1 (5/5), Tier 2 (7/7), Tier 3 (4/5), Tier 4 (2/5). Session 229 added UltraPlan, Commands Catalog, Feature Flags. 3 remaining: voice, API layer, messages, UI renderer. Reports: `AI_Studio/Reports/ClaudeCodeInternals/`
- **VoxSniffer v1.0.0**: SHIPPED (session 168). 7-round dual ChatGPT review (API + Browser). 62 files, 8,881 lines. Deployed to GitHub + AddOns + publishable/. **NEEDS IN-GAME TEST**
- **VoxGM v1.0.0**: SHIPPED (session 167). **NEEDS IN-GAME TEST**
- **CreatureCodex v1.0.0**: RELEASED (session 171b). Build clean. GitHub release live. **NEEDS IN-GAME TEST**
- **DraconicBot v3**: Standalone repo, Gemini AI enabled, Oracle VM provisioned, not yet deployed
- **VoxCore Daemon**: Phase 1 COMPLETE, Phase 2 next (LogMonitor, ReportWriter, InboxTriage)
- **Release Gate System**: DEPLOYED. All 8 claude-code-* repos v1.0.0 released. vNext MCP spec ready
- **Brand Expansion**: Strategy defined (session 171c). Website is #1 blocker. awesome-claude-code submission sent (awaiting maintainer response). Reddit outreach plan ready (14 comment drafts across 26 threads). mvanhorn PR contribution posted (#32755)

## Inbox Status (14 files — +2 Warlock/VoxSniffer intakes in 1_Inbox, +2 approved specs in 2_Active_Specs)
Potentially actionable specs remaining in `1_Inbox/`:
- 3x build-66337 specs (CASC, EXTRACT, WAGO)
- 2x catalog specs (enterprise catalog, pilot)
- Command Center unified UI spec
- Social Monitor V1 spec
- 3x DraconicBot specs (SmartFAQ, v3 architecture, v3 Gemini spec request)
- 1x FAQ regex seed JSON

39 stale infrastructure specs archived to `4_Archive/Triad_Infrastructure/`.
2 sensitive personal files moved to `Desktop\Excluded/`.

## Infrastructure State
- **Build**: Current (VS build done, 867/867)
- **Server**: RUNNING (PID 41144, 28s startup). 670K DBErrors (down from 6.8M — 90% reduction)
- **Client**: 12.0.1.66709
- **DB**: world ~1,200 MB | hotfixes 811 MB | characters 4 MB
- **31 slash commands** (+12 case management skills session 178)
- **Cowork**: OPERATIONAL with 5 scheduled tasks
- **Bridge**: `cowork/sync_bridge.py` — auto-runs on `/wrap-up`

## Upcoming / Unassigned Backlog
- Sweep `VoxCore\doc\` directory for deprecated files
- ~~Gemini API key setup~~ DONE (session 169 — all 3 API keys configured, review cycle operational)
- VoxCore Daemon Phase 2
- DraconicBot v3 Oracle Cloud deployment
- CreatureCodex in-game testing (build done, release live)
