---
description: "glossary codenames — Tengu feature flags, Chicago computer use, Buddy tamagotchi, Kairos autonomous, Moreright, DXT extensions, beta headers, hard limits, model IDs"
---

# Glossary & Internal Codenames — Arcanum Wiki

> Every internal term, codename, and abbreviation used in Claude Code's source code.

## Codenames

| Codename | What It Is | Source Location |
|----------|-----------|-----------------|
| **Tengu** | Internal prefix for ALL feature flags and analytics events. Appears hundreds of times. Uses bird-species suffixes (e.g., `tengu_onyx_plover`, `tengu_hawthorn_window`) | `services/analytics/growthbook.ts` |
| **Chicago** | Computer Use feature — screenshot capture, click coordinates, autonomous computer control | `utils/computerUse/` (13 files) |
| **Buddy** | Tamagotchi virtual pet Easter egg — care for a digital pet with personality | `src/buddy/` (6 files) |
| **Kairos** | Autonomous agent mode — proactive AI that takes initiative without prompting. Feature-gated | `src/proactive/`, `tools/BriefTool/` |
| **AutoDream** | Background memory consolidation — runs after 24h + 5 sessions. 4-phase process (Orient, Gather, Consolidate, Prune) | `services/autoDream/`, `tasks/DreamTask/` |
| **Moreright** | Unknown internal feature — directory exists at `src/moreright/` | `src/moreright/` |
| **DXT** | Extension system — third-party plugin framework | `utils/dxt/` |
| **Teleport** | Feature for navigating between sessions/contexts | `utils/teleport/` |
| **MagicDocs** | Service that provides contextual documentation | `services/MagicDocs/` |
| **Thinkback** | Replay/review of Claude's thinking process | `commands/thinkback/`, `commands/thinkback-play/` |
| **YOLO** | The auto-permission classifier — decides what's safe to auto-approve | `utils/permissions/` |
| **Grove** | UI component system (name found in `components/grove/`) | `components/grove/` |
| **Undercover** | Mode where Claude Code hides its identity as Claude Code | `utils/undercover.ts` |

## Technical Terms

| Term | Meaning |
|------|---------|
| **Microcompact** | Lightweight compaction that clears old tool results without an LLM call. Two variants: API microcompact (server-side) and client-side microcompact |
| **SM-compact** | Session Memory compact — preserves 10K-40K tokens verbatim during compaction |
| **Full LLM compact** | Complete conversation summarization using an LLM call. Most expensive, most thorough |
| **Fork subagent** | Agent spawning mode that shares the parent's prompt cache prefix for massive cache hits |
| **In-process teammate** | Swarm agent running in the same Node.js process (vs Tmux/iTerm backends) |
| **Prompt cache** | Anthropic API feature — repeated prompt prefixes are cached and don't re-incur token costs |
| **Tool search** | Deferred tool loading — only includes tool definitions in prompt when the model requests them. Saves prompt tokens |
| **Permission bridge** | How swarm leaders relay permission decisions to worker agents |
| **Channel push** | MCP server feature for pushing notifications through 6-layer gating |
| **Managed settings** | Enterprise-level settings that override user preferences. Cannot be changed locally |
| **Conditional rules** | `.claude/rules/` files with `paths:` frontmatter that only activate when matching files are touched |
| **Dynamic boundary** | `__SYSTEM_PROMPT_DYNAMIC_BOUNDARY__` marker separating cacheable from non-cacheable system prompt content |
| **Attribution header** | `x-anthropic-billing-header` sent with every API request. Contains version + fingerprint + entrypoint + optional attestation token |
| **Client attestation (cch)** | Native binary attestation token. Bun's HTTP stack overwrites a `cch=00000` placeholder with a computed hash to prove the request came from a real Claude Code binary |
| **Workload routing** | `cc_workload` tag in attribution header — routes cron-initiated requests to lower QoS API pool |

## System Prompt Sections

| Section | What It Contains |
|---------|-----------------|
| **System** | Tool permissions, system-reminder tags, hook feedback, compression notice |
| **Doing tasks** | Software engineering instructions, code style, over-engineering warnings |
| **Executing actions with care** | Reversibility checks, blast radius, risky action confirmation |
| **Using your tools** | Tool selection guidance (dedicated tools over Bash), agent usage, parallelism |
| **Tone and style** | No emojis, concise, file:line citations |
| **Auto memory** | Memory directory location, what/how to save, correction protocol |
| **Environment** | CWD, platform, shell, OS, model name, knowledge cutoff |
| **MCP Server Instructions** | Instructions provided by connected MCP servers |
| **claudeMd** | All CLAUDE.md content merged from root to CWD (injected as user message, NOT system prompt) |
| **gitStatus** | Git branch, remote, status (capped at 2K chars) |

## Feature Flags (Tengu Prefix)

| Flag | What It Gates |
|------|--------------|
| `tengu_onyx_plover` | AutoDream background consolidation |
| `tengu_hawthorn_window` | Per-message tool result budget override (`MAX_TOOL_RESULTS_PER_MESSAGE_CHARS`) |
| `tengu_session_memory` | Session memory feature |
| `tengu_attribution_header` | Attribution header (killswitch — enabled by default) |

## Build-Time Feature Flags

These are compile-time `feature()` checks from `bun:bundle` — dead code elimination:

| Flag | What It Gates |
|------|--------------|
| `CACHED_MICROCOMPACT` | Cached microcompact config |
| `PROACTIVE` | Proactive agent features |
| `KAIROS` | Kairos autonomous mode + Brief tool |
| `KAIROS_BRIEF` | Brief tool only (subset of Kairos) |
| `EXPERIMENTAL_SKILL_SEARCH` | DiscoverSkills tool + skill search service |
| `NATIVE_CLIENT_ATTESTATION` | Binary attestation in attribution header |
| `CONNECTOR_TEXT` | Connector text summarization beta |
| `TRANSCRIPT_CLASSIFIER` | AFK mode transcript classification |

## Beta Headers

Every beta header sent to the Anthropic API (from `constants/betas.ts`):

| Constant | Header Value | Purpose |
|----------|-------------|---------|
| `CLAUDE_CODE_20250219_BETA_HEADER` | `claude-code-20250219` | Core Claude Code features |
| `INTERLEAVED_THINKING_BETA_HEADER` | `interleaved-thinking-2025-05-14` | Extended thinking |
| `CONTEXT_1M_BETA_HEADER` | `context-1m-2025-08-07` | 1M token context window |
| `CONTEXT_MANAGEMENT_BETA_HEADER` | `context-management-2025-06-27` | Context management |
| `STRUCTURED_OUTPUTS_BETA_HEADER` | `structured-outputs-2025-12-15` | Structured outputs |
| `WEB_SEARCH_BETA_HEADER` | `web-search-2025-03-05` | Web search tool |
| `TOOL_SEARCH_BETA_HEADER_1P` | `advanced-tool-use-2025-11-20` | Tool search (1P API) |
| `TOOL_SEARCH_BETA_HEADER_3P` | `tool-search-tool-2025-10-19` | Tool search (Vertex/Bedrock) |
| `EFFORT_BETA_HEADER` | `effort-2025-11-24` | Effort/reasoning control |
| `TASK_BUDGETS_BETA_HEADER` | `task-budgets-2026-03-13` | Task budget limits |
| `PROMPT_CACHING_SCOPE_BETA_HEADER` | `prompt-caching-scope-2026-01-05` | Cache scope control |
| `FAST_MODE_BETA_HEADER` | `fast-mode-2026-02-01` | Fast output mode |
| `REDACT_THINKING_BETA_HEADER` | `redact-thinking-2026-02-12` | Thinking redaction |
| `TOKEN_EFFICIENT_TOOLS_BETA_HEADER` | `token-efficient-tools-2026-03-28` | Compact tool definitions |
| `SUMMARIZE_CONNECTOR_TEXT_BETA_HEADER` | `summarize-connector-text-2026-03-13` | Connector text (feature-gated) |
| `AFK_MODE_BETA_HEADER` | `afk-mode-2026-01-31` | AFK/autonomous mode (feature-gated) |
| `CLI_INTERNAL_BETA_HEADER` | `cli-internal-2026-02-09` | Anthropic employees only |
| `ADVISOR_BETA_HEADER` | `advisor-tool-2026-03-01` | Advisor tool |

## Hard Limits (from constants/)

### Tool Results (`constants/toolLimits.ts`)
| Constant | Value | Meaning |
|----------|-------|---------|
| `DEFAULT_MAX_RESULT_SIZE_CHARS` | 50,000 | Single tool result cap before disk persistence |
| `MAX_TOOL_RESULT_TOKENS` | 100,000 | Token-based tool result cap |
| `BYTES_PER_TOKEN` | 4 | Conservative bytes-per-token estimate |
| `MAX_TOOL_RESULT_BYTES` | 400,000 | Derived from token limit (100K * 4) |
| `MAX_TOOL_RESULTS_PER_MESSAGE_CHARS` | 200,000 | Aggregate cap for parallel tool results in one turn |
| `TOOL_SUMMARY_MAX_LENGTH` | 50 | Max chars for tool summary in compact views |

### API Limits (`constants/apiLimits.ts`)
| Constant | Value | Meaning |
|----------|-------|---------|
| `API_IMAGE_MAX_BASE64_SIZE` | 5 MB | Max base64 image size |
| `IMAGE_TARGET_RAW_SIZE` | 3.75 MB | Max raw image size (before base64) |
| `IMAGE_MAX_WIDTH` / `IMAGE_MAX_HEIGHT` | 2,000 px | Client-side image resize limit |
| `PDF_TARGET_RAW_SIZE` | 20 MB | Max raw PDF size |
| `API_PDF_MAX_PAGES` | 100 | Max PDF pages |
| `PDF_MAX_PAGES_PER_READ` | 20 | Max pages per Read tool call |
| `PDF_EXTRACT_SIZE_THRESHOLD` | 3 MB | PDFs above this extracted to images |
| `PDF_MAX_EXTRACT_SIZE` | 100 MB | Absolute max PDF size |
| `PDF_AT_MENTION_INLINE_THRESHOLD` | 10 | PDFs with more pages get reference treatment |
| `API_MAX_MEDIA_PER_REQUEST` | 100 | Max images + PDFs per API call |

### Model IDs
| Model | ID |
|-------|-----|
| Opus 4.6 | `claude-opus-4-6` |
| Sonnet 4.6 | `claude-sonnet-4-6` |
| Haiku 4.5 | `claude-haiku-4-5-20251001` |
| Frontier label | `Claude Opus 4.6` |
