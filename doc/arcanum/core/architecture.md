---
description: "architecture overview — source directory map, data flows, entry points CLI SDK server bridge, query engine main loop, tool execution pipeline, supporting systems"
---

# Architecture Overview — Arcanum Wiki

> The complete system architecture of Claude Code v0.2.57+, reverse-engineered from 1,884 TypeScript source files.

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    ENTRY POINTS                             │
│  CLI (main)  │  SDK  │  Server Mode  │  Bridge (IDE)       │
└──────┬───────┴───┬───┴───────┬───────┴──────┬──────────────┘
       │           │           │              │
       ▼           ▼           ▼              ▼
┌─────────────────────────────────────────────────────────────┐
│                   BOOTSTRAP / SETUP                         │
│  Config migrations │ Settings load │ Analytics init         │
│  GrowthBook flags  │ OAuth check   │ MCP server discovery   │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                  QUERY ENGINE (Main Loop)                    │
│                                                             │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌───────────┐  │
│  │ Context  │  │ System   │  │ Message  │  │ API Call  │  │
│  │ Assembly │→ │ Prompt   │→ │ Pipeline │→ │ + Stream  │  │
│  └──────────┘  └──────────┘  └──────────┘  └─────┬─────┘  │
│                                                    │        │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐        │        │
│  │ Tool     │← │ Response │← │ Stream   │←───────┘        │
│  │ Executor │  │ Parser   │  │ Handler  │                  │
│  └────┬─────┘  └──────────┘  └──────────┘                  │
│       │                                                     │
│       ▼                                                     │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                  │
│  │ Tool     │  │Permission│  │ Hook     │                  │
│  │ Registry │→ │ Check    │→ │ Pipeline │→ Execute         │
│  └──────────┘  └──────────┘  └──────────┘                  │
└─────────────────────────────────────────────────────────────┘
       │              │               │
       ▼              ▼               ▼
┌─────────────────────────────────────────────────────────────┐
│                    SUPPORTING SYSTEMS                        │
│                                                             │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌───────────┐  │
│  │ Memory   │  │ Compactn │  │ Skills   │  │ MCP       │  │
│  │ Pipeline │  │ Engine   │  │ System   │  │ Client    │  │
│  └──────────┘  └──────────┘  └──────────┘  └───────────┘  │
│                                                             │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌───────────┐  │
│  │ Agent    │  │ Swarm    │  │ Tasks    │  │ AutoDream │  │
│  │ Coordntr │  │ System   │  │ Manager  │  │ Service   │  │
│  └──────────┘  └──────────┘  └──────────┘  └───────────┘  │
└─────────────────────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────────────────────┐
│                   TERMINAL UI (Ink/React)                    │
│  Prompt Input │ Streaming Output │ Diff View │ Permissions  │
│  Status Line  │ Spinner         │ Agent UI  │ MCP Status   │
└─────────────────────────────────────────────────────────────┘
```

## Source Directory Map

### Top Level (`src/`)

| Directory | Files | Purpose |
|-----------|-------|---------|
| `assistant/` | ~5 | Assistant message handling and response processing |
| `bootstrap/` | ~8 | Application initialization, state setup, first-run detection |
| `bridge/` | 31 | IDE integration (VS Code, JetBrains) via bridge protocol |
| `buddy/` | 6 | Tamagotchi virtual pet Easter egg |
| `cli/` | ~15 | CLI argument parsing, transport handling |
| `commands/` | 189 | All slash commands (one directory per command) |
| `components/` | ~80 | React/Ink UI components |
| `constants/` | 21 | Shared constants (limits, betas, prompts, tool names) |
| `context/` | ~10 | Context assembly for API calls |
| `coordinator/` | ~15 | Agent orchestration and subagent management |
| `entrypoints/` | ~8 | Main entry points (CLI, SDK, server) |
| `hooks/` | 104 | Hook pipeline and event handling |
| `ink/` | ~30 | Terminal UI renderer (Ink framework) |
| `keybindings/` | ~5 | Keyboard shortcut definitions |
| `memdir/` | ~5 | Memory directory operations |
| `migrations/` | ~10 | Config format migrations |
| `moreright/` | ~3 | Unknown internal feature |
| `native-ts/` | ~15 | Native TypeScript helpers (color-diff, file-index, yoga-layout) |
| `outputStyles/` | ~5 | Output formatting presets |
| `plugins/` | ~10 | Plugin system and bundled plugins |
| `query/` | ~10 | Query handling and routing |
| `remote/` | ~8 | Remote/cloud session features |
| `schemas/` | ~10 | JSON schemas for settings, MCP config, etc. |
| `screens/` | ~5 | UI screens (onboarding, settings) |
| `server/` | ~10 | Server mode (headless API) |
| `services/` | ~60 | Internal services (API, MCP, OAuth, analytics, compact, etc.) |
| `skills/` | ~20 | Skill loading, execution, and bundled skills |
| `state/` | ~10 | Application state management |
| `tasks/` | ~15 | Background tasks (Dream, Shell, Agent, Teammate) |
| `tools/` | 184 | Tool registration and execution (one dir per tool + shared infra) |
| `types/` | ~10 | TypeScript type definitions |
| `upstreamproxy/` | ~5 | Upstream proxy handling |
| `utils/` | ~200 | Core utilities (largest section — see below) |
| `vim/` | ~10 | Vim mode implementation |
| `voice/` | ~10 | Voice/push-to-talk feature |

### Utils Breakdown (`src/utils/`)

| Directory | Purpose |
|-----------|---------|
| `background/` | Background processing, task scheduling |
| `bash/` | Bash tool implementation details |
| `claudeInChrome/` | Chrome extension integration |
| `computerUse/` | Computer Use / "Chicago" (13 files) |
| `deepLink/` | Deep linking between sessions |
| `dxt/` | DXT extension system |
| `filePersistence/` | File persistence layer for large tool results |
| `git/` | Git operations (status, diff, branch) |
| `github/` | GitHub integration (PR, issues) |
| `hooks/` | Hook utilities and execution |
| `mcp/` | MCP client implementation |
| `memory/` | Memory pipeline (selector, extraction, storage) |
| `messages/` | Message assembly pipeline for API calls |
| `model/` | Model selection, configuration, providers |
| `nativeInstaller/` | Native installer for different platforms |
| `permissions/` | Permission evaluation engine |
| `plugins/` | Plugin utilities |
| `powershell/` | PowerShell integration |
| `processUserInput/` | User input processing pipeline |
| `sandbox/` | Sandboxing (file system restrictions) |
| `secureStorage/` | Credential storage |
| `settings/` | Settings management (load, save, merge) |
| `shell/` | Shell operations and command execution |
| `skills/` | Skill discovery and loading utilities |
| `suggestions/` | Autocomplete suggestions |
| `swarm/` | Multi-agent swarm system (20+ files) |
| `task/` | Task management utilities |
| `telemetry/` | Analytics and telemetry |
| `teleport/` | Teleport feature |
| `todo/` | Todo list management |
| `ultraplan/` | UltraPlan feature |

## Key Data Flows

### 1. User Input → API Response

```
User types message
  → processUserInput (input processing)
  → Context assembly (memory selector runs, rules loaded)
  → System prompt built (static sections + dynamic boundary + env-specific)
  → CLAUDE.md injected as user message (NOT system prompt)
  → Git status appended (2K char cap)
  → API call constructed (headers, betas, model selection)
  → Streaming response received
  → Response parsed for tool calls
  → If tool calls: execute tools → append results → loop back to API
  → If text: render to terminal via Ink
```

### 2. Tool Execution Pipeline

```
Model requests tool call
  → Tool registry lookup (40+ registered tools)
  → Permission check (8 rule sources, 7 modes)
  → Hook pipeline (PreToolUse → decision)
  → If denied: return denial to model
  → If approved: execute tool
  → Result size check (50K char / 100K token cap)
  → If too large: persist to disk, return preview + path
  → Per-message aggregate check (200K chars across parallel tools)
  → Hook pipeline (PostToolUse)
  → Return result to model
```

### 3. Compaction Pipeline

```
Token count approaches threshold (83% of context)
  → Tier 1: API microcompact (server-side, if supported)
  → Tier 2: Client microcompact (clear old tool results)
  → Tier 3: Session memory compact (preserve 10K-40K tokens verbatim)
  → Tier 4: Full LLM compact (summarize entire conversation)
  → PreCompact/PostCompact hooks fire
  → Up to 5 files restored post-compact (50K budget, 5K/file)
```

### 4. Memory Selection (per turn)

```
Turn starts
  → Sonnet side-query reads all memory filenames + description frontmatter
  → Selects up to 5 most relevant topic files
  → MEMORY.md always loaded (200 lines / 25KB cap)
  → Selected files' content injected into context
  → Files beyond 200 (by mtime) are invisible to selector
```

## Entry Points

| Entry Point | File | Use Case |
|------------|------|----------|
| CLI (interactive) | `entrypoints/cli.ts` | Normal `claude` terminal usage |
| SDK | `entrypoints/sdk.ts` | Claude Agent SDK integration |
| Server | `src/server/` | Headless API mode |
| Bridge | `src/bridge/` | IDE integration (VS Code extension) |

## Key Singletons / Global State

| What | Where | Scope |
|------|-------|-------|
| Settings | `utils/settings/settings.ts` | Merged from 7 sources at startup |
| GrowthBook | `services/analytics/growthbook.ts` | Feature flags, cached values |
| MCP connections | `services/mcp/` | Per-session, lazy-initialized |
| Tool registry | `src/tools.ts` | All tools registered at startup |
| Permission state | `utils/permissions/` | Includes consecutive-denial tracker |
| Memory | `memdir/` + `utils/memory/` | Memory directory operations |

## Cross-References

- [Glossary](glossary.md) — all codenames and internal terms
- [System Prompt](system_prompt.md) — how the prompt is assembled
- [Tool Pipeline](../tools/pipeline_overview.md) — tool execution details
- [Compaction Tiers](compaction_tiers.md) — compaction deep dive
- [Memory Selector](memory_selector.md) — memory file selection algorithm
