---
description: "AgentTool internals — agent spawning, fork mode, coordinator orchestration, subagent delegation, background agent execution, agent context passing"
---

# Agent Tool Internals -- Arcanum Wiki

## Overview

The Agent tool is the single entry point for all subagent spawning in Claude Code. It encompasses three distinct orchestration modes: normal mode (the default, where the main agent delegates to typed subagents), fork mode (cache-sharing children that inherit full conversation context), and coordinator mode (a top-level orchestrator that only manages workers). The tool handles schema validation, permission checks, agent definition resolution, sync/async dispatch, worktree isolation, and result delivery including safety classification.

## How It Works

### Input Schema

The Agent tool accepts these parameters (from `src/tools/AgentTool/AgentTool.tsx`):

| Parameter | Type | Description |
|-----------|------|-------------|
| `description` | string (required) | 3-5 word task summary for UI |
| `prompt` | string (required) | Full task instructions |
| `subagent_type` | string (optional) | Agent type. Omit for fork mode (when enabled) |
| `model` | enum (optional) | `sonnet`, `opus`, `haiku` override |
| `run_in_background` | boolean (optional) | Run asynchronously |
| `name` | string (optional) | Addressable name for SendMessage routing |
| `team_name` | string (optional) | Team context for swarm spawning |
| `isolation` | enum (optional) | `worktree` or `remote` |

### Dispatch Logic

When the model invokes the Agent tool, the dispatch path depends on the `subagent_type` parameter and active feature gates:

```
subagent_type provided?
  YES -> Look up agent definition by type
       -> Resolve tools, permissions, MCP servers
       -> Run as typed subagent (sync or async)
  NO  -> Fork gate enabled?
       YES -> Fork mode (cache-sharing child)
       NO  -> Default to "general-purpose" agent type
```

In coordinator mode, ALL spawns are forced async. The model parameter is forcibly set to `undefined` (coordinator system prompt handles guidance).

### Agent Definition Sources (Priority Order)

Agents are loaded from six sources; later sources override earlier ones for same-named types:

```
1. built-in        -- Explore, Plan, General, Guide, Statusline, Verification
2. plugin          -- From installed plugins
3. userSettings    -- ~/.claude/agents/*.md
4. projectSettings -- .claude/agents/*.md
5. flagSettings    -- GrowthBook/feature flags
6. policySettings  -- Enterprise managed policy
```

Custom agents are defined as markdown files with YAML frontmatter in `.claude/agents/` directories. The markdown body becomes the system prompt.

### Sync vs Async Execution

**Sync path**: The `runAgent()` iterator is consumed in the foreground. After 2 seconds, a "background hint" UI appears (Shift+Down to background). On completion, `finalizeAgentTool()` extracts the result.

**Async path**: `registerAsyncAgent()` creates a task entry, the iterator runs fire-and-forget, and the model immediately receives `{ status: 'async_launched', agentId, outputFile }`. On completion, a `<task-notification>` XML message is delivered to the parent conversation.

**Auto-background**: If `CLAUDE_AUTO_BACKGROUND_TASKS` is set, sync agents automatically transition to background after 120 seconds.

### Handoff Safety Classification

When a subagent completes (in `auto` permission mode), its output is safety-checked via `classifyHandoffIfNeeded()`. Three outcomes:
- **Allowed**: Output passed through normally
- **Blocked**: Security warning prepended
- **Unavailable**: Softer warning about classifier being down

### Agent Cleanup

Every agent (sync or async) cleans up in a `finally` block: MCP connections, session hooks, prompt cache tracking, file state cache, initial messages array, Perfetto trace entries, transcript mappings, TodoWrite entries, and background bash tasks.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/tools/AgentTool/AgentTool.tsx` | Tool definition, schema, dispatch logic |
| `src/tools/AgentTool/runAgent.ts` | Agent execution loop, tool filtering, MCP, cleanup |
| `src/tools/AgentTool/forkSubagent.ts` | Fork mode gate, message construction, recursive guard |
| `src/tools/AgentTool/loadAgentsDir.ts` | Agent definition loading from all sources |
| `src/tools/AgentTool/builtInAgents.ts` | Built-in agent registry |
| `src/tools/AgentTool/agentToolUtils.ts` | Tool filtering, async lifecycle, handoff classification |
| `src/coordinator/coordinatorMode.ts` | Coordinator mode detection and system prompt |

## Cross-References

- [Subagent Types](subagent_types.md) -- Built-in agent types and custom definitions
- [Fork Mode](fork_mode.md) -- Cache-sharing fork architecture
- [Swarm Overview](../agents/swarm_overview.md) -- Multi-agent team system

## Interesting Findings

**Explore agents save 5-15 Gtok/week by dropping CLAUDE.md.** Both Explore and Plan agents set `omitClaudeMd: true`, which excludes CLAUDE.md content and git status from their context. At 34M Explore spawns per week fleet-wide, this is a significant cost reduction.

**ONE_SHOT_BUILTIN_AGENT_TYPES skip the continuation trailer.** Explore and Plan agents never get continued via SendMessage, so their tool_result omits the agentId/SendMessage/usage trailer, saving ~135 chars per invocation.

**Agent tool prompt is dynamic and mode-aware.** The tool description changes based on whether fork mode, coordinator mode, or standard mode is active, and whether the agent list is injected inline or via attachment messages.
