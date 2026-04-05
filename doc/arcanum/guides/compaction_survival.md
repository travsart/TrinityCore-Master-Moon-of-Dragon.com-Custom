---
description: "surviving compaction — 4 tiers, what context survives, compaction instructions, checkpoint, post-compact restore budget, 1M context delay"
---

# Guide: Surviving Compaction — Arcanum Wiki

> How to ensure critical context survives when Claude Code compresses your conversation.

## What Is Compaction?

When your conversation approaches the context window limit (~83% full), Claude Code automatically compresses older messages to make room. This is called compaction. Without it, you'd hit the context wall and have to start a new session.

The problem: compaction can lose important context. Understanding HOW it works lets you design your workflow to survive it.

## The 4 Compaction Tiers

Compaction fires in escalating tiers:

### Tier 1: API Microcompact (Server-Side)
- **What**: The API server clears its internal cache of old tool results
- **Impact on you**: None — your conversation is unchanged
- **Triggered by**: API-side heuristics

### Tier 2: Client Microcompact
- **What**: Old tool results in your conversation are replaced with summaries
- **Impact on you**: You lose the exact output of tools from many turns ago
- **What survives**: The summary captures the key information
- **No LLM call**: This is deterministic, fast, and free

### Tier 3: Session Memory Compact (SM-compact)
- **What**: Preserves 10K-40K tokens verbatim from recent conversation, summarizes the rest
- **Impact on you**: Recent work is preserved exactly. Older work is summarized.
- **Feature-gated**: `tengu_session_memory`

### Tier 4: Full LLM Compact
- **What**: The entire conversation is summarized by an LLM call
- **Impact on you**: Maximum compression. Everything becomes a summary.
- **Most expensive**: Costs tokens for the summarization call
- **Your Compaction Instructions are used here**

## What Survives Compaction

### Always survives:
- System prompt (rebuilt fresh)
- CLAUDE.md content (re-injected)
- Rules files (re-loaded)
- Memory files (re-selected by Sonnet)
- Git status (re-queried)
- MCP server connections (persistent)

### Sometimes survives:
- Recent conversation turns (Tier 3 preserves 10K-40K tokens verbatim)
- Tool results from recent turns (Tier 2 replaces old ones with summaries)

### Often lost:
- Exact tool output from early in the conversation
- Specific file contents you read 20+ turns ago
- Detailed error messages from early debugging
- Nuanced context about WHY you made certain decisions

### Restored post-compact:
- Up to 5 memory files (50K total budget, 5K per file)
- These are re-selected by the Sonnet memory selector

## Compaction Instructions

The most powerful tool you have. Add to CLAUDE.md:

```markdown
## Compaction Instructions
When compacting, ALWAYS preserve:
1. Files modified this session with their paths
2. Current task/goal and progress
3. Pending SQL or build actions that haven't been applied
4. Spawned agents and their findings
5. Any error messages being debugged
6. Database state changes made this session

Drop: exploration results, failed approaches, verbose tool output,
file contents that can be re-read.
```

The full LLM compact (Tier 4) specifically reads this section and uses it as guidance for what to keep. Without it, the LLM makes its own judgment about what matters.

## Strategies for Critical Sessions

### 1. Use /checkpoint for Long Sessions

Before a major topic shift or after 30+ minutes of deep work, use `/checkpoint` to snapshot your state. This writes current context to a persistent file that survives compaction.

### 2. Write Findings to Disk Immediately

Don't accumulate findings in conversation and "write them up at the end." If compaction fires before you write, the findings are summarized (possibly lossy).

**Pattern**:
```
Research something → Write findings to file IMMEDIATELY → Continue
```

### 3. Use Memory Files as Anchors

Memory files survive compaction because they're re-loaded. If you discover something critical mid-session, write it to a memory file:

```
"Update memory/db-schema-notes.md with: creature_template.faction is NOT FactionID"
```

Now that fact survives any compaction.

### 4. Keep Sessions Focused

The #1 cause of compaction is sprawling sessions that cover multiple unrelated topics. Each topic generates tool results that consume context.

**Pattern**: One focused objective per session. Use multi-tab for parallel work.

### 5. Use Background Agents for Heavy Research

Subagents have their OWN context windows. When an agent returns, only its result (not its full internal conversation) enters your context. This is dramatically more context-efficient than doing the research yourself.

**Pattern**:
```
Agent researches 50 files → Returns 200-line summary → YOUR context only grows by 200 lines
vs.
You read 50 files yourself → YOUR context grows by 50,000+ lines
```

### 6. Re-Read After Compaction

After compaction fires, Claude may have lost details about files you modified. Use Read to re-check critical files rather than relying on memory:

```
"I may have lost context. Let me re-read the key files before continuing."
```

## Detecting Compaction

You'll know compaction happened when:
1. Claude suddenly seems to have forgotten context from earlier
2. The system shows a "conversation compressed" notification
3. Claude asks about things you already discussed
4. Context cost drops significantly (check with `/cost`)

## The 50K Post-Compact Budget

After compaction, Claude Code restores context by re-loading memory files. The budget:
- **Total**: 50,000 tokens across all restored files
- **Per file**: 5,000 token cap
- **File count**: Up to 5 files selected by Sonnet

This means if your MEMORY.md is 200 lines (~800 tokens) and 5 topic files are each ~1,000 tokens, you use ~5,800 of 50K. That's efficient.

But if you have huge topic files (500+ lines each), they'll be truncated at 5K tokens each.

## Compaction Hooks

You can run custom code before and after compaction:

```json
{
  "hooks": {
    "PreCompact": [
      {
        "matcher": "",
        "hooks": [
          {
            "type": "command",
            "command": "python .claude/hooks/pre_compact_save.py"
          }
        ]
      }
    ],
    "PostCompact": [
      {
        "matcher": "",
        "hooks": [
          {
            "type": "command",
            "command": "python .claude/hooks/post_compact_restore.py"
          }
        ]
      }
    ]
  }
}
```

Use cases:
- **PreCompact**: Save critical state to a file before it's lost
- **PostCompact**: Inject a reminder of what was being worked on

## The Nuclear Option: 1M Context

With 1M context (`opus[1m]`), compaction fires at ~967K tokens instead of ~167K. Most sessions will NEVER hit this threshold. A typical session uses 50K-200K tokens.

1M context is included free on the Max plan. There's essentially no reason not to enable it.

## Cross-References

- [Compaction Overview](../core/compaction_overview.md) — full technical architecture
- [Compaction Tiers](../core/compaction_tiers.md) — each tier in detail
- [Context Window](optimizing_context.md) — context optimization
- [Memory System](memory_mastery.md) — how memory files interact with compaction
