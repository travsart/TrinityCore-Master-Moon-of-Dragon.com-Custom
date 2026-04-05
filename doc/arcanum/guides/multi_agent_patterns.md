---
description: "multi-agent patterns — fan-out research, parallel implementation, adversarial review, team swarm, fork cache sharing, 50-message cap, cost optimization"
---

# Guide: Multi-Agent Patterns That Actually Work — Arcanum Wiki

> Proven patterns for using subagents, swarms, and teams effectively, based on source code understanding.

## Why Multi-Agent?

Claude Code's subagent system is the single most powerful feature for complex tasks. Understanding the internals lets you exploit it:

1. **Subagents have their OWN context windows** — heavy research stays in their context, only the result enters yours
2. **Fork mode shares prompt cache** — spawning 5 agents costs ~1.2x one agent, not 5x
3. **Parallel execution** — with 16C/32T, multiple agents run simultaneously
4. **Specialized tools** — Explore agents are read-only (can't accidentally modify), Plan agents can't edit

## Pattern 1: Fan-Out Research

**Use when**: You need to understand a large codebase, search many files, or investigate multiple angles.

```
Main context: "Search the codebase for all database query patterns"
  ├── Agent 1 (Explore): "Search src/services/ for SQL patterns"
  ├── Agent 2 (Explore): "Search src/utils/ for SQL patterns"
  ├── Agent 3 (Explore): "Search src/tools/ for SQL patterns"
  └── Agent 4 (Explore): "Search src/hooks/ for SQL patterns"
```

**Why it works**: Each agent reads dozens of files in its own context. Your context only grows by ~4 result summaries instead of ~200 file contents.

**Key setting**: Launch all 4 in a single message with `run_in_background: true` for true parallelism.

### Source Code Insight

Fork subagents (`tools/AgentTool/forkSubagent.ts`) share the parent's prompt cache prefix. The first agent pays full price; agents 2-4 get massive cache hits on the shared prefix (system prompt + CLAUDE.md + conversation history).

## Pattern 2: Research → Implement Pipeline

**Use when**: You need to understand something before implementing it.

```
Phase 1: Agent (Explore) researches the problem
  → Returns findings
Phase 2: You synthesize findings + design solution
Phase 3: Agent (code-writer) or you implement
```

**Why it works**: Separating research from implementation prevents the "I read 50 files and now I've forgotten what I'm building" problem. The research agent's full context stays fresh during its search, and only the distilled findings enter your implementation context.

## Pattern 3: Parallel Implementation

**Use when**: Changes touch independent files/systems.

```
Agent 1: "Implement the Python script at tools/warlock/extract.py"
Agent 2: "Write the SQL migration at sql/updates/world/..."
Agent 3: "Update the CLAUDE.md documentation"
```

**Why it works**: These agents write to different files. No conflicts. All three run in parallel.

**Danger**: If agents try to edit the same file, you get conflicts. Split by FILE, not by logical concern.

## Pattern 4: Adversarial Review (Pre-Ship)

**Use when**: You need quality assurance before shipping.

```
Agent 1 (app-reviewer, persona=noob): "Try to use this addon as a beginner"
Agent 2 (app-reviewer, persona=bully): "Find everything wrong with this code"
Agent 3 (app-reviewer, persona=security): "Audit for security vulnerabilities"
```

**Why it works**: Each agent has a different perspective. The "noob" finds UX issues the builder is blind to. The "bully" finds code quality issues. The "security auditor" finds vulnerabilities.

**This is exactly how `/pre-ship` works internally.**

## Pattern 5: Team Swarm

**Use when**: Complex multi-step project with task dependencies.

```
TeamCreate("feature-x")
  → TaskCreate: "Research existing implementation"
  → TaskCreate: "Design new architecture"
  → TaskCreate: "Implement core module"
  → TaskCreate: "Write tests"
  → TaskCreate: "Update documentation"

Spawn teammates:
  → researcher (Explore agent)
  → implementer (code-writer agent)
  → documenter (general-purpose agent)

Assign tasks → teammates work independently
Teammates mark tasks complete → next tasks unblock
```

**Source**: Teams use `~/.claude/teams/{name}/config.json` for member tracking and `~/.claude/tasks/{name}/` for shared task lists.

**Key limit**: In-process teammates cap at 50 messages each. For long-running work, use foreground agents instead.

## Pattern 6: Warm-Up Cache

**Use when**: You're about to do a lot of work in one area.

```
Agent (Explore): "Read all files in src/tools/BashTool/ and summarize the architecture"
→ Result enters your context
→ Now YOUR subsequent tool calls to those files benefit from understanding
```

This is less about the agent and more about getting a concise summary in your context before you start working.

## Anti-Patterns

### Don't: Sequential Agents for Independent Work
```
BAD:
Agent 1: search src/tools/ → wait → Agent 2: search src/hooks/ → wait → Agent 3: ...

GOOD:
Agent 1 + Agent 2 + Agent 3: all launched in same message, all run in parallel
```

### Don't: Agent for Simple Lookups
```
BAD: Agent to "find where class Foo is defined"
GOOD: Glob("**/Foo.ts") or Grep("class Foo")
```

Agents have startup overhead. For a single search, use Glob/Grep directly.

### Don't: Huge Agent Prompts Without File Paths
```
BAD: "Find the configuration files somewhere in the project"
GOOD: "Read C:/Users/atayl/VoxCore/.claude/settings.local.json and summarize the hooks"
```

Give agents exact file paths when you know them. Saves them from searching.

### Don't: Let Agents Accumulate Then Write
```
BAD: "Research these 50 files and write a report at the end"
GOOD: "Research these 50 files and write findings to reports/findings.md as you go"
```

If an agent hits context limits or crashes, accumulated-but-unwritten findings are lost.

## Optimal Agent Configuration

### For Research
```json
{
  "subagent_type": "Explore",
  "run_in_background": true,
  "model": "sonnet"  // Cheaper for research
}
```

### For Implementation
```json
{
  "subagent_type": "code-writer",
  "mode": "bypassPermissions",  // Don't ask for every edit
  "run_in_background": true
}
```

### For Review
```json
{
  "subagent_type": "general-purpose",
  "run_in_background": true,
  "isolation": "worktree"  // Isolated git copy
}
```

## Cost Considerations

| Approach | Cost | Context Impact |
|----------|------|----------------|
| Read 50 files yourself | Low API cost | ~200K tokens in YOUR context |
| 5 Explore agents in parallel | ~1.5x API cost | ~5K tokens in your context (just results) |
| 1 Explore agent | ~1.1x API cost | ~1K tokens in your context |

The multi-agent approach costs slightly more in API tokens but DRAMATICALLY reduces context pressure on your main conversation. For long sessions, this is a net win.

## The 50-Message Teammate Cap

In-process teammates (the fastest swarm backend) are hard-capped at 50 messages. This was a response to a 36.8GB RSS memory explosion in production.

**Workaround**: For long-running team tasks, break work into phases. Each teammate completes its batch of 5-10 tasks, gets replaced with a fresh teammate for the next batch.

## Cross-References

- [Agent/Coordinator](../agents/coordinator_overview.md) — how Agent tool works
- [Swarm System](../agents/swarm_overview.md) — team infrastructure
- [Fork Mode](../agents/fork_mode.md) — cache sharing
- [Skills & Agents Guide](skills_and_agents.md) — defining custom agents
