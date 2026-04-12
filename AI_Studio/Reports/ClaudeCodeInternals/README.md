# Claude Code Internals -- Knowledge Base

Source baseline: v0.2.57 + v2.1.88 npm packages (sourcemap extraction, March-April 2026)
**2.1.97 refresh overlay**: `cli.js@2.1.97` grep-verification (minified bundle, no sourcemap — Anthropic stopped shipping `cli.js.map` after the 2.1.88 leak)
Local archive: `C:/Users/atayl/Desktop/claude-code-source/` (both `claude-code-source/` v2.1.88 tree and `extract-2.1.97/package/` tarball)
Status: **ALL 22 REPORTS COMPLETE** -- Tier 1 through Tier 4 fully documented + **2.1.97 delta refresh applied 2026-04-08**

## Session 232 — 2.1.97 Refresh (2026-04-08)

### What was done

1. Pulled `@anthropic-ai/claude-code@2.1.97` tarball via `npm pack` — discovered the sourcemap is gone (Anthropic stripped it after the 2.1.88 leak).
2. User-approved Option A: **Hybrid changelog + cli.js grep** methodology. Every new claim is anchored by a grep match against `cli.js@2.1.97` (13.4 MB minified) or a line in `sdk-tools.d.ts@2.1.97` or a v2.1.88 source citation for contrast. Full methodology: `_2.1.97_refresh/methodology.md`.
3. Fanned out 14 parallel Explore/general-purpose agents across 3 waves covering all 22 reports. Each agent wrote structured findings incrementally to `_2.1.97_refresh/findings_*.md` rather than accumulating in context.
4. Main tab applied surgical Edit-based delta summaries to the top of each report (preserves existing content, surfaces changes in one findable block per report). Memory file `memory/claude-code-internals.md` updated with top-level invalidations/gaps + codename table.

### Biggest invalidations (things the reports were wrong about before the refresh)

- **TaskOutput tool deprecated (2.1.83)** — `shouldDefer:!0`, runtime description says "[Deprecated]". AgentOutput/BashOutputTool alias into it. Report 06.
- **Agent `resume` parameter removed (2.1.77)** — use `SendMessage` auto-resume. Reports 06, 08.
- **`CLAUDE_CODE_MAX_CONTEXT_TOKENS` REMOVED** in 2.1.97 (zero grep hits). `getContextWindowForModel` simplified from 7 priorities to 4. Reports 03, 1m_context.
- **Buddy system REMOVED in 2.1.97** post-April-Fools. Report 15 is historical.
- **MCP HTTP/SSE leak ~50 MB/hour fixed in 2.1.97** — vendored SDK upgrade. Report 12.
- **Compaction wrote duplicate subagent transcript files** (fixed 2.1.97). Report 01.
- **Subagent worktree CWD leak** (fixed 2.1.97). Report 07.
- **1M context is NOT default for Enterprise/plain Team** — the changelog overreached. Only Max and Team Premium (`E16()` = team with `default_claude_max_5x`) get `Opus 4.6[1m]` default. Reports 03, 1m_context.

### Biggest gaps (new surface area added to reports)

- **`defer` permission decision** + **Hook output persistence** + **TaskCreated/TaskCompleted hooks** → Report 09.
- **`CLAUDE_CODE_NO_FLICKER` alt-screen compositor** (~56 `T4()` call sites) → Report 22 Section 3b.
- **NEW `getAutoCompactWindow` resolver** with 4 sources + **rapid-refill breaker** → Reports 01, 03.
- **Dream: Memory Pruning** prompt + tiny-memory-mode → Report 05.
- **`/powerup`** + removal of `/tag`/`/vim`/`/pr-comments`/`/output-style` + internal-only shrinkage → Report 18.
- **MCP per-tool `maxResultSizeChars`, CIMD (SEP-991), RFC 9728, `MCP_CONNECTION_NONBLOCKING`, Slack MCP UI override** → Report 12.
- **PowerShell opt-in preview + `disableSkillShellExecution` + Accept-Edits env-var prefixes + `forceRemoteSettingsRefresh` + `managed-settings.d/`** → Reports 06, 10.
- **6 provider paths** (was 4, added Mantle + anthropicAws) + **429 Retry-After cap** + **long-retry visibility fix** → Report 19.
- **Voice Nova 3 unconditional + startup banner → spinner tip** → Report 14.
- **UltraPlan timeout 30→90 min + prompt-template selector** → Report 16.

### Methodology note for the next refresh wave

The v2.1.88 sourcemap leak was a one-time accident (published 2026-03-31, removed immediately). Every version since (including 2.1.97 shipped 2026-04-08) omits `cli.js.map`. All public GitHub mirrors (AprilNEA, hangsman, laddyladboy, ChinaSiro/Hyper66666) are frozen at 2.1.88. **Do not attempt re-extraction via source-map recovery for future refreshes** — use the hybrid changelog + grep approach documented in `_2.1.97_refresh/methodology.md`.

---

## Session 230 Summary (Apr 5 2026)

## Session 222 Summary (Apr 4 2026)

### What Was Done
1. Deep SME exploration of v2.1.88 source (1,902 TS files) via 6 parallel agents
2. Wrote all 7 Tier 2 reports (tool pipeline, swarm, coordinator, hooks, permissions, skills, MCP)
3. Updated memory/claude-code-internals.md with comprehensive architecture map

### Session 220 Summary (Apr 3 2026)
1. Downloaded 4 source archives (162MB, 1,884 TypeScript files) before DMCA takedowns
2. Mapped all 22 major systems across 213 directories
3. Wrote 5 Tier 1 deep-dive reports (compaction, system prompt, context, memory, dream)
4. Wrote 6 optimization reports (settings, session memory, frontmatter, rules, gitignore, 1M context)
5. Applied optimizations to live config (1M context, conditional rules, git status, memory frontmatter)

### Session 230 Summary (Apr 5 2026)
1. Wrote final 4 reports via parallel agents: voice (14), API layer (19), messages pipeline (20), UI renderer (22)
2. All 22 reports now complete across all 4 tiers
3. Updated README and memory files

### What's Next
- Consider building a knowledge base MCP server for instant recall
- Cross-reference reports for optimization opportunities
- Upload to NotebookLM for human study

---

## Report Index

### Tier 1 -- Core Mechanics (COMPLETE)
| Report | System | Key Finding |
|--------|--------|-------------|
| [01_compaction_engine.md](01_compaction_engine.md) | Compaction | 4-tier system. Auto-compact at 167K (200K) or 967K (1M). Honors `## Compaction Instructions` |
| [02_system_prompt_assembly.md](02_system_prompt_assembly.md) | System Prompt | CLAUDE.md is NOT in system prompt -- injected as first user message with "may or may not be relevant" hedging |
| [03_context_window.md](03_context_window.md) | Context Window | Opus 4.6 supports 1M via `[1m]` suffix. No local tokenizer -- uses API countTokens |
| [04_memory_pipeline.md](04_memory_pipeline.md) | Memory | Sonnet selector only sees filenames + `description` frontmatter. 200 file cap. 200 line / 25KB MEMORY.md limit |
| [05_autodream.md](05_autodream.md) | AutoDream | Background consolidation after 24h + 5 sessions. 4-phase prompt. Gated by `tengu_onyx_plover` |

### Optimization Reports (COMPLETE)
| Report | Focus | Key Finding |
|--------|-------|-------------|
| [1m_context_deep_dive.md](1m_context_deep_dive.md) | 1M Context | Max plan includes Opus 1M at no extra cost. `[1m]` suffix is client-side only |
| [settings_deep_dive.md](settings_deep_dive.md) | All Settings | 70+ settings keys documented. 10 env vars in our config were no-ops |
| [session_memory_deep_dive.md](session_memory_deep_dive.md) | Session Memory | Feature-gated (`tengu_session_memory`). Custom templates at `~/.claude/session-memory/config/` |
| [rules_optimization.md](rules_optimization.md) | Conditional Rules | 3 files made conditional. Transmog moved. 46% fewer tokens on non-code turns |
| [gitignore_optimization.md](gitignore_optimization.md) | Git Status | 205 -> 15 untracked entries. Git status fits in 2K cap now |
| [frontmatter_summary.md](frontmatter_summary.md) | Memory Frontmatter | 54 files updated with keyword-rich descriptions for Sonnet selector |

### Tier 2 -- Power User (COMPLETE)
| Report | System | Key Finding |
|--------|--------|-------------|
| [06_tool_pipeline.md](06_tool_pipeline.md) | Tool Registration & Execution | 40+ tools via `buildTool()` factory. Concurrent read-only batching (max 10). Results >100K persisted to disk. ToolSearch defers tools to save prompt tokens |
| [07_swarm_system.md](07_swarm_system.md) | Multi-Agent Swarm | 3 backends (tmux, iTerm2, in-process). Mailbox messaging. Leader permission bridge. 50-message UI cap per teammate |
| [08_coordinator.md](08_coordinator.md) | Agent Orchestration | Fork mode shares prompt cache. 6 built-in agents. Coordinator mode restricts to 4 tools. Agent defs are markdown+YAML |
| [09_hooks_system.md](09_hooks_system.md) | Hook Pipeline | 27 hook events, 4 command types (shell/prompt/agent/http). 4-way permission race. Hook allow does NOT bypass deny rules |
| [10_permissions.md](10_permissions.md) | Permission Evaluation | 7 modes, 8 rule sources. YOLO classifier for auto mode. Dangerous pattern stripping. Enterprise managed-only controls |
| [11_skills_system.md](11_skills_system.md) | Skill Loading & Execution | 8 sources, 17 bundled skills. Dynamic discovery from file edits. Conditional `paths` activation. 6 cache layers |
| [12_mcp_client.md](12_mcp_client.md) | MCP Server Integration | 7 config scopes, 8 transports. OAuth PKCE. Channel push system with 6-layer gating. ~27.8h tool timeout |

### Tier 3 -- Hidden Features (COMPLETE)
| Report | System | Key Finding |
|--------|--------|-------------|
| [13_computer_use.md](13_computer_use.md) | Computer Use ("Chicago") | macOS-only, triple-layer gating (compile+GrowthBook+subscription). 23 tools via in-process MCP. Rust/enigo input + Swift screenshots. CFRunLoop pump every 1ms for libuv compat |
| [14_voice_mode.md](14_voice_mode.md) | Voice / Push-to-Talk | Input-only (STT, no TTS). Triple-gated (compile+GrowthBook+OAuth). Deepgram Nova 3 via Anthropic WS proxy. Hold-to-talk via 200ms auto-repeat gap detection. 50 project keyterms for accuracy boosting |
| [15_buddy_system.md](15_buddy_system.md) | Tamagotchi Pet | 18 species, deterministic gacha from hash(userId). 1% shiny, 1% legendary. Anti-cheat: bones regenerated on read, only soul persists. April 1 2026 launch |
| [16_ultraplan.md](16_ultraplan.md) | UltraPlan | Internal-only remote planning via CCR. Opus 4.6, 30-min timeout, 3 entry points (slash/keyword/plan-upgrade), 27 files. Keyword rainbow animation. Not usable externally |
| [17_bridge.md](17_bridge.md) | IDE Integration / Remote Control | Cloud-mediated (not direct IDE-CLI). Two protocols: v1 (WS+POST via Environments API) and v2 (SSE+CCRClient, env-less). Permission delegation, crash recovery, multi-session spawn |

### Tier 4 -- Infrastructure (COMPLETE)
| Report | System | Key Finding |
|--------|--------|-------------|
| [18_commands_catalog.md](18_commands_catalog.md) | All Slash Commands (~90) | 55 active, 18 stubbed, ~14 feature-gated, ~25 internal-only. /insights is 119KB analytics platform. /security-review has 3-step parallel analysis. Kairos appears in 6 flags |
| [19_api_layer.md](19_api_layer.md) | API Calls & Streaming | Raw streams over SDK (avoids O(n^2) partial JSON). 10 retries, exponential backoff. 529 cascade protection. Opus→Sonnet fallback after 3 consecutive 529s. 15+ beta headers with session-stable latches. 1-hour prompt cache TTL |
| [20_messages_pipeline.md](20_messages_pipeline.md) | Message Assembly | Two-zone system prompt (static cacheable + dynamic session). CLAUDE.md injected as synthetic user msg at position 0. 11-transform normalization. ~30 concurrent attachment providers. Tool results >50K persisted to disk with preview |
| [21_feature_flags.md](21_feature_flags.md) | Feature Flags & "Tengu" | ~175+ total gates (92 build-time + 60+ runtime + 25+ env). GrowthBook replacing Statsig. Kairos = unreleased scheduling platform. Chicago = Computer Use. Anti-distillation fake tool injection |
| [22_ui_renderer.md](22_ui_renderer.md) | Ink/React Terminal | Custom Ink fork with pure-TS Yoga layout. Cell-level double-buffered rendering at 60fps. REPL.tsx is 5,005-line mega-component. charCache + blit optimization. LRU markdown cache (500 entries) |

### Installed Config Files
| File | Location |
|------|----------|
| Session memory template | `~/.claude/session-memory/config/template.md` |
| Session memory prompt | `~/.claude/session-memory/config/prompt.md` |

---

## Critical Findings (Always Reference)

### Tier 1 (Core)
1. **CLAUDE.md is user context, not system prompt** -- wrapped in `<system-reminder>` with "may or may not be relevant"
2. **Memory selector only sees filenames + frontmatter `description`** -- content is never read during selection
3. **1M context is free on Max plan** -- `opus[1m]` suffix, auto-compact at 967K
4. **Conditional rules save ~2,823 tokens/turn** on non-code work
5. **Git status capped at 2K chars** -- .gitignore hygiene directly affects every session
6. **Session memory templates are customizable** but feature-gated
7. **AutoDream consolidates memory in background** -- 4-phase process, 24h + 5 session gate

### Tier 2 (Power User)
8. **Hook `allow` does NOT bypass deny/ask rules** -- defense-in-depth, hooks are additive only
9. **Tool results >100K chars persisted to disk** -- model gets preview + file path, not full content
10. **Fork subagents share parent prompt cache** -- byte-identical API prefix = massive cache hit
11. **In-process teammates capped at 50 messages** -- prevents RSS explosion (learned from 36.8GB incident)
12. **Skills support conditional `paths` activation** -- only loaded when matching files are touched
13. **MCP tool timeout defaults to ~27.8 hours** -- effectively infinite, override via `MCP_TOOL_TIMEOUT`
14. **YOLO classifier tracks consecutive denials** -- falls back to prompting after threshold
15. **Dynamic skill discovery** -- editing a file triggers walk-up search for `.claude/skills/` directories

### Tier 3 (Hidden Features)
16. **Computer Use is macOS-only and Max/Pro-gated** -- No Windows/Linux. Triple gate: compile-time + GrowthBook `tengu_malort_pedway` + subscription tier. Cannot be enabled externally
17. **MCP tool naming matters for CU** -- Tools MUST be `mcp__computer-use__*` because the API backend detects this prefix and injects a system prompt hint. The name `computer-use` is reserved
18. **Buddy companion is deterministic and cheat-proof** -- Bones regenerated from hash(userId) on every read. Config stores only soul (name+personality). Editing config cannot fake a rarity
19. **Bridge is cloud-mediated, not direct IDE-CLI** -- All traffic routes through Anthropic CCR servers. Works across networks (mobile controlling laptop)
20. **Remote Control has bidirectional permission delegation** -- Remote user can approve/deny tools, change models, switch permission modes, and interrupt
21. **drainRunLoop pattern for Swift/Rust under Node** -- 1ms setInterval pumping CFRunLoop is the solution for DispatchQueue.main calls under libuv

### Tier 3 (continued)
22. **Voice mode is STT-only, no TTS** -- Deepgram Nova 3 via Anthropic WebSocket proxy. Triple-gated like Computer Use (ant builds only)
23. **Hold-to-talk detects key release via 200ms auto-repeat gap** -- Terminals lack keyUp events, so CC uses auto-repeat timing heuristic
24. **50 project keyterms sent to Deepgram** -- Project name, git branch, coding terms boost transcription accuracy

### Tier 4 (Infrastructure)
25. **Raw SSE streams, not SDK BetaMessageStream** -- Avoids O(n^2) partial JSON parsing in the SDK's accumulator
26. **Opus→Sonnet automatic fallback after 3 consecutive 529s** -- Prevents cascade amplification during overload
27. **15+ beta headers with session-stable latches** -- Latches prevent prompt cache breaks from mid-session feature flag changes
28. **CLAUDE.md is synthetic user message at position 0** -- Injected via `prependUserContext()`, not part of system prompt array
29. **~30 concurrent attachment providers per tool-use** -- Primary dynamic context injection mechanism (skills, MCP, memory, git status, etc.)
30. **Tool results >50K chars persisted to disk** -- Model gets `<persisted-output>` preview + file path; 200K aggregate cap per message
31. **Custom Ink fork with pure-TS Yoga layout** -- Replaced upstream WASM Yoga with single-pass TypeScript flexbox. 60fps rendering with cell-level diffing
32. **REPL.tsx is 5,005 lines** -- Single mega-component managing entire interactive session (streaming, permissions, vim, dialogs, agents, costs)

## Source Archive

```
C:/Users/atayl/Desktop/claude-code-source/
  claude-code-source/                    -- Clean extraction (primary reference, 44MB)
  collection-claude-code-source-code/    -- Curated collection + rewrites (108MB)
  claurst/                               -- Rust rewrite + analysis (9.6MB)
  anthropic-ai-claude-code-0.2.57.tgz   -- Raw npm package (12MB)
```
