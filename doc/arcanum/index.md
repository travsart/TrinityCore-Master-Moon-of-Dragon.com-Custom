---
description: "Arcanum wiki index — Claude Code internals knowledge base, 296 files, 25 directories, cross-reference to ClaudeCodeInternals reports, build status"
---

# Arcanum — Claude Code Internals Wiki

> A comprehensive reverse-engineered knowledge base of Claude Code's internal architecture, systems, and hidden features. Built from leaked v0.2.57 source code (1,884 TypeScript files) and live behavior analysis.

**Source archive**: `C:/Users/atayl/Desktop/claude-code-source/`
**Existing deep reports**: `AI_Studio/Reports/ClaudeCodeInternals/` (18 reports, 266KB)
**Purpose**: NotebookLM knowledge base + future MCP server for persistent tab recall

---

## Directory Structure

| Directory | Contents | Target Docs |
|-----------|----------|-------------|
| `core/` | Core mechanics — compaction, context, memory, system prompt, AutoDream | ~15 |
| `tools/` | Individual tool reference — one doc per built-in tool | ~40 |
| `hooks/` | Hook system — events, protocol, pipeline, examples | ~10 |
| `permissions/` | Permission evaluation — modes, rules, YOLO, globs | ~8 |
| `agents/` | Agent orchestration — coordinator, subagents, swarm, teams | ~12 |
| `skills/` | Skill system — discovery, loading, execution, bundled | ~8 |
| `mcp/` | MCP client — servers, transports, channels, OAuth | ~8 |
| `commands/` | Slash command catalog — one doc per command group | ~25 |
| `ui/` | Terminal UI — Ink/React renderer, components, layout | ~10 |
| `bridge/` | IDE integration — VS Code, protocol, state sync | ~8 |
| `hidden/` | Hidden features — Buddy, voice, computer use, UltraPlan | ~12 |
| `config/` | Configuration reference — settings, env vars, feature flags | ~10 |
| `guides/` | Practical how-to guides for power users | ~15 |
| `api/` | API layer — calls, streaming, retry, token counting | ~8 |
| `services/` | Internal services — analytics, OAuth, tips, summaries | ~10 |
| `source/` | Key source file analysis — root files, constants | ~5 |
| `internals/` | Boot sequence, state management, entry points | ~3 |
| `query/` | The main loop — query.ts, streaming, tool dispatch | ~3 |
| `networking/` | Remote execution, server mode, proxy handling | ~3 |
| `sandbox/` | Sandboxing, bash execution, shell management | ~2 |
| `git/` | Git operations, GitHub integration | ~2 |
| `telemetry/` | Analytics events, cost tracking, privacy | ~2 |
| `plugins/` | Plugin system, DXT extensions, bundled plugins | ~2 |
| `limits/` | Rate limiting, usage tracking, passes | ~2 |

**Target: ~260 documents** (25 folders)

---

## Cross-Reference: Existing Reports

The 18 reports in `AI_Studio/Reports/ClaudeCodeInternals/` are the foundation. Arcanum expands each into multiple focused articles and adds entirely new coverage areas.

| Existing Report | Arcanum Expansion |
|----------------|-------------------|
| 01_compaction_engine.md | core/compaction_overview.md + core/compaction_tiers.md + core/compaction_hooks.md |
| 02_system_prompt_assembly.md | core/system_prompt.md + core/claude_md_injection.md + core/context_assembly.md |
| 03_context_window.md | core/context_window.md + core/token_counting.md + core/1m_context.md |
| 04_memory_pipeline.md | core/memory_overview.md + core/memory_selector.md + core/memory_frontmatter.md + core/memory_limits.md |
| 05_autodream.md | core/autodream.md + core/autodream_phases.md |
| 06_tool_pipeline.md | tools/pipeline_overview.md + tools/[per_tool].md (40 docs) |
| 07_swarm_system.md | agents/swarm_overview.md + agents/swarm_backends.md + agents/swarm_messaging.md |
| 08_coordinator.md | agents/coordinator.md + agents/subagent_types.md + agents/fork_mode.md |
| 09_hooks_system.md | hooks/overview.md + hooks/[per_event].md |
| 10_permissions.md | permissions/overview.md + permissions/modes.md + permissions/yolo.md |
| 11_skills_system.md | skills/overview.md + skills/discovery.md + skills/bundled.md |
| 12_mcp_client.md | mcp/overview.md + mcp/transports.md + mcp/oauth.md + mcp/channels.md |

---

## Build Status

### Populated (have content)
- [x] Core mechanics — 12 articles (compaction, memory, context, rules, etc.)
- [x] Commands catalog — 12 articles (categorized by function)
- [x] Hidden features — 11 articles (buddy, voice, computer use, ultraplan, etc.)
- [x] Services — 14 articles (compact, memories, oauth, plugins, etc.)
- [x] Tools — 17 articles (one per tool + pipeline overview)
- [x] Agents — 3 articles (coordinator, fork mode, subagent types)
- [x] API layer — 3 articles (overview, context, messages)
- [x] Guides — 6 articles (hooks, mcp, memory, context, permissions, skills)

### Stubbed (folder + key questions, needs research)
- [ ] Bridge/IDE — 3 stubs (architecture, websocket, session lifecycle)
- [ ] Config — 3 stubs (settings registry, migrations, schema validation)
- [ ] Hooks — 3 stubs (events catalog, execution pipeline, permission race)
- [ ] MCP — 3 stubs (connection lifecycle, transports, oauth pkce)
- [ ] Permissions — 3 stubs (evaluation order, yolo classifier, rule sources)
- [ ] Skills — 3 stubs (loading pipeline, bundled, conditional activation)
- [ ] Internals — 3 stubs (boot sequence, state management, entry points) **NEW**
- [ ] Query — 3 stubs (query loop, streaming, tool dispatch) **NEW**
- [ ] Networking — 3 stubs (remote execution, server mode, proxy) **NEW**
- [ ] Sandbox — 2 stubs (overview, bash execution) **NEW**
- [ ] Git — 2 stubs (operations, github integration) **NEW**
- [ ] Telemetry — 2 stubs (analytics events, cost tracking) **NEW**
- [ ] Plugins — 2 stubs (plugin system, dxt extensions) **NEW**
- [ ] Limits — 2 stubs (rate limiting, usage tracking) **NEW**
- [ ] Source — 2 stubs (root files, constants/prompts) **NEW**
- [ ] UI — 2 stubs (ink renderer, component tree) **NEW** (low priority)
