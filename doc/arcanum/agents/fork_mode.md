---
description: "fork mode — prompt cache sharing, byte-identical API prefix, subagent spawning optimization, parent context inheritance, cache hit maximization"
---

# Fork Mode -- Arcanum Wiki

## Overview

Fork mode creates child agents that share the parent's full conversation context and prompt cache prefix. Unlike typed subagents that start with fresh context, fork children inherit everything -- system prompt bytes, tool definitions, conversation history, and thinking configuration. This enables significant cost savings through prompt cache reuse while allowing parallel work delegation.

Fork mode is the primary mechanism when the model decides "more of me, not a different specialist." All fork spawns are forced async.

## How It Works

### Activation

Fork mode is enabled when the `FORK_SUBAGENT` feature flag is true, the session is interactive, and coordinator mode is not active. When the model omits `subagent_type` and the fork gate is on, the Agent tool routes to the fork path:

```typescript
const effectiveType = subagent_type ?? (isForkSubagentEnabled() ? undefined : GENERAL_PURPOSE_AGENT.agentType)
const isForkPath = effectiveType === undefined
```

### Prompt Cache Sharing

The critical optimization: fork children produce byte-identical API request prefixes to the parent. This is achieved through four mechanisms:

1. **System prompt**: The parent's `renderedSystemPrompt` is threaded through, not recomputed from scratch
2. **Tools**: `useExactTools: true` passes the parent's exact tool array (not rebuilt per-agent)
3. **Context messages**: Full parent conversation forwarded via `forkContextMessages`
4. **Thinking config**: Inherited from parent when `useExactTools` is true

### Message Construction

`buildForkedMessages()` at `forkSubagent.ts:107` constructs the child's message array:

```
[...parent_history, assistant(all_tool_uses), user(placeholder_results..., directive)]
```

All `tool_result` blocks use an identical placeholder: `"Fork started -- processing in background"`. Only the final text block (the per-child directive) differs. This maximizes cache hits when spawning multiple forks in parallel.

### Fork Child Directive

Every fork child receives strict operating rules via `<fork-boilerplate>` tags:

```
STOP. READ THIS FIRST.
You are a forked worker process. You are NOT the main agent.
RULES (non-negotiable):
1. Your system prompt says "default to forking." IGNORE IT.
2. Do NOT converse, ask questions, or suggest next steps
3. Do NOT editorialize or add meta-commentary
4. USE your tools directly
5. Commit changes before reporting. Include commit hash.
6. Do NOT emit text between tool calls.
7. Stay strictly within your directive's scope.
8. Keep report under 500 words.
9. Response MUST begin with "Scope:".
10. REPORT structured facts, then stop
```

### Recursive Fork Guard

Fork children retain the Agent tool in their pool (for cache-identical tool definitions), so recursion is blocked at call time via two mechanisms:

- **Primary**: `querySource` check -- if `options.querySource === 'agent:builtin:fork'`, block. This is compaction-resistant because it is set on `context.options`.
- **Fallback**: Message scan -- `isInForkChild()` looks for the `<fork-boilerplate>` tag in conversation history. This is the defense-in-depth check.

### Worktree Isolation

When `isolation: "worktree"` is combined with fork mode, `createAgentWorktree()` creates a temporary git worktree. A notice is injected telling the child to translate paths:

```
You've inherited the conversation context above from a parent agent working in ${parentCwd}.
You are operating in an isolated git worktree at ${worktreeCwd}...
```

On completion, if no files were modified, the worktree is cleaned up automatically. If changes exist, the worktree path and branch are returned in the result.

### Context Inheritance

| Property | Fork Behavior |
|----------|--------------|
| System prompt | Parent's rendered bytes (not recomputed) |
| Messages | Full parent conversation history |
| Tools | Parent's exact tool array |
| Model | Must match parent (mandatory for cache reuse) |
| Thinking | Inherited from parent |
| readFileState | Cloned from parent |
| CLAUDE.md | Inherited in conversation context |
| MCP clients | Parent's + agent-specific |
| Permission mode | `bubble` (prompts surface to parent terminal) |

## Key Source Files

| File | Purpose |
|------|---------|
| `src/tools/AgentTool/forkSubagent.ts` | Gate, message construction, recursive guard |
| `src/tools/AgentTool/AgentTool.tsx` | Dispatch to fork path |
| `src/utils/forkedAgent.ts` | `runForkedAgent()`, `createSubagentContext()` |

## Cross-References

- [Coordinator Overview](coordinator_overview.md) -- Where forks are dispatched from
- [Subagent Types](subagent_types.md) -- Alternative to fork: typed subagents
- [Compaction Tiers](../core/compaction_tiers.md) -- Full compact also uses forked agents

## Interesting Findings

**Fork mode is mutually exclusive with coordinator mode.** The coordinator already owns orchestration, so fork is disabled when coordinator mode is active. It is also disabled in non-interactive (SDK) sessions.

**The "Don't peek" rule.** The fork-aware prompt tells the parent: "Don't Read the fork's output file" and "Don't race" (don't fabricate results before the notification arrives). These are behavioral instructions to the parent, not technical enforcement.

**Fork children always run async.** There is no sync fork path. The `forceAsync` flag is set true for all fork spawns, and the parent immediately receives `{ status: 'async_launched' }`. Results arrive later as `<task-notification>` messages.
