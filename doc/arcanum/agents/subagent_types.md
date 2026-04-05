---
description: "subagent types — built-in agent definitions, agent YAML frontmatter, tool restrictions per type, specialized agent capabilities, custom .claude/agents/"
---

# Subagent Types -- Arcanum Wiki

## Overview

Claude Code ships with six built-in agent types, each optimized for a specific task category. Custom agents can be defined via markdown files with YAML frontmatter in `.claude/agents/` directories. All agent types share the `AgentDefinition` base type but differ in tools, model selection, permission modes, and system prompts.

## How It Works

### Built-In Agent Types

**General Purpose** (`general-purpose`) -- The default when no `subagent_type` is specified and fork mode is disabled. Has access to all tools, uses the default subagent model, and provides a generic task-completion system prompt. From `generalPurposeAgent.ts`.

**Explore** -- Fast codebase search and code questions. Restricted to read-only tools (all except Agent, ExitPlanMode, FileEdit, FileWrite, NotebookEdit). Uses Haiku model for external users, `inherit` for ant users. Sets `omitClaudeMd: true` to save context tokens. The system prompt emphasizes speed and parallel tool calls. Gated by `BUILTIN_EXPLORE_PLAN_AGENTS` + `tengu_amber_stoat`.

**Plan** -- Software architecture and implementation planning. Same read-only tool set as Explore. Uses `inherit` model, `omitClaudeMd: true`. System prompt focuses on step-by-step strategies with critical file lists. Read-only mode ensures no accidental modifications during planning.

**Claude Code Guide** (`claude-code-guide`) -- Answers questions about Claude Code itself ("how do I...", "can Claude..."). Limited to Glob, Grep, Read, WebFetch, WebSearch. Uses Haiku with `dontAsk` permission mode (never prompts). Dynamic system prompt includes the user's custom skills, agents, MCP servers, and settings.

**Statusline Setup** (`statusline-setup`) -- Configures the user's status line display. Limited to Read and Edit tools only. Uses Sonnet, assigned the `orange` color.

**Verification** (`verification`) -- Adversarial post-implementation verification. Read-only tools only. Uses `inherit` model. Always runs in background (`background: true`). Assigned the `red` color. The system prompt defines structured PASS/FAIL/PARTIAL verdicts with command output evidence and explicitly combats "verification avoidance" failure patterns. A critical reminder is re-injected every turn: "This is a VERIFICATION-ONLY task. You CANNOT edit files." Gated by `VERIFICATION_AGENT` + `tengu_hive_evidence`.

### Custom Agent Definition Format

Custom agents are markdown files at `.claude/agents/<name>.md`:

```yaml
---
name: reviewer
description: Code review specialist
tools:
  - Read
  - Grep
  - Glob
  - Bash(gh:*)
model: opus
effort: high
permissionMode: default
maxTurns: 50
color: blue
context: fork
---

# Code Review Agent

Review the provided code for bugs, style issues, and security concerns.
```

The markdown body becomes the system prompt. If `memory` is set (`user`, `project`, or `local`), `loadAgentMemoryPrompt()` is appended.

### Agent-Specific MCP Servers

Agents can declare `mcpServers` in their frontmatter. Two forms are supported:
- **String reference** (e.g., `"slack"`): Shares an existing MCP connection
- **Inline definition** (e.g., `{ my-server: { command: "..." } }`): Creates a new connection, cleaned up when the agent finishes

### Tool Filtering

Three disallow/allow sets control what tools an agent can access:

- **ALL_AGENT_DISALLOWED_TOOLS**: TaskOutput, ExitPlanModeV2, EnterPlanMode, AskUserQuestion, TaskStop, Workflow, Agent (for external users)
- **ASYNC_AGENT_ALLOWED_TOOLS**: Read, WebSearch, Grep, WebFetch, Glob, Bash, Edit, Write, NotebookEdit, Skill, and a few more
- The `Agent(type1, type2)` syntax in tool specs restricts which agent types can be spawned

### Permission Mode Override

Agent-defined permission modes are respected unless the parent is in `bypassPermissions`, `acceptEdits`, or `auto` mode -- those parent modes always take precedence.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/tools/AgentTool/builtInAgents.ts` | Registry for all 6 built-in agents |
| `src/tools/AgentTool/loadAgentsDir.ts` | Loading from all 6 sources, markdown parsing |
| `src/tools/AgentTool/agentToolUtils.ts` | `filterToolsForAgent()`, `resolveAgentTools()` |
| `src/constants/tools.ts` | Disallow/allow sets |

## Cross-References

- [Coordinator Overview](coordinator_overview.md) -- How agents are spawned
- [Fork Mode](fork_mode.md) -- The fork agent type
- [Swarm Overview](swarm_overview.md) -- Team-based agent spawning

## Interesting Findings

**Agent color assignment is per-type, not per-instance.** Eight colors are available: red, blue, green, yellow, purple, orange, pink, cyan. Colors are assigned by agent type and stored in a global map. The `general-purpose` type explicitly returns `undefined` for color.

**MCP requirement filtering gates agent visibility.** Agents can declare `requiredMcpServers` patterns. Before appearing in the available list, the system checks whether matching MCP servers are connected and have tools. At call time, a 30-second wait is applied for pending MCP servers.
