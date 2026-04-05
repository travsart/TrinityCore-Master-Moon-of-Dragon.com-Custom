---
description: "skills agents swarm guide — custom slash commands, skill frontmatter paths conditional, agent definitions, fork mode cache, team lifecycle, 50-message cap"
---

# Guide: Skills, Agents & the Swarm — Arcanum Wiki

> How to build custom skills, define agents, and leverage the multi-agent swarm system.

## Skills: Custom Slash Commands

Skills are user-defined slash commands. When you type `/my-skill`, Claude Code discovers, loads, and executes it as a prompt expansion.

### Skill Discovery (8 Sources)

Claude Code discovers skills from these locations, in priority order:

1. **Bundled skills** — 17 built into Claude Code itself
2. **Project `.claude/skills/`** — checked into your repo
3. **User `~/.claude/skills/`** — global personal skills
4. **Walk-up search** — when a file is edited, Claude walks up parent directories looking for `.claude/skills/`
5. **MCP tool-provided skills** — MCP servers can register skills
6. **Plugin-provided skills** — DXT plugins can define skills
7. **Dynamic discovery** — editing a file triggers automatic re-scan
8. **Skill search** — feature-gated deferred discovery

### Skill File Format

Skills are markdown files with YAML frontmatter:

```markdown
---
description: "Build the project and fix errors iteratively"
paths:
  - "src/**/*.cpp"
  - "src/**/*.h"
---

# Build Loop

Build the project using `ninja -j32`. If there are compilation errors,
fix them and rebuild. Repeat until the build succeeds or you've tried
3 times.

## Steps
1. Run the build
2. Parse errors
3. Fix each error
4. Rebuild
```

### Key Frontmatter Fields

| Field | Type | Purpose |
|-------|------|---------|
| `description` | string | Shown in `/skills` list. Used by Sonnet selector |
| `paths` | string[] | Conditional activation — skill only appears when these file patterns are in context |
| `args` | string | Argument description (shown to user) |
| `user_invocable` | boolean | If true, appears in `/` autocomplete. Default: true |
| `model` | string | Override model for this skill (e.g., `sonnet` for cheaper execution) |

### Conditional Activation (`paths:`)

Skills with `paths:` frontmatter are only loaded when matching files are touched in the session. This saves prompt tokens:

```yaml
---
paths:
  - "sql/**/*.sql"
---
# SmartAI Check
Validate SmartAI SQL...
```

This skill only appears when SQL files are involved. On a C++ session, it's invisible.

**Source**: `utils/skills/loadSkills.ts` — `paths` matched against files in current context.

### Skill Execution

When you invoke `/my-skill`:
1. Claude Code finds the matching `.md` file
2. Reads its content (the markdown body, not frontmatter)
3. Injects it into the conversation as a system message
4. Claude sees and follows the skill instructions
5. The skill prompt replaces the user's message for that turn

### Built-in Skills (17)

From the source at `src/skills/bundled/`:

| Skill | What It Does |
|-------|-------------|
| `/commit` | Git commit with structured message |
| `/review` | Code review |
| `/init` | Project initialization |
| `/help` | Help and documentation |
| `/memory` | Memory management |
| And 12 more... | Various utilities |

## Agents: Specialized Subprocesses

Agents are defined as markdown files in `.claude/agents/` and describe specialized roles that subagents can take on.

### Agent Definition Format

```markdown
---
name: code-writer
description: "Implement C++ changes — writes scripts, modifies game systems"
tools:
  - Read
  - Write
  - Edit
  - Grep
  - Glob
  - Bash
  - mcp__codeintel__*
model: sonnet
---

# Code Writer Agent

You implement C++ changes in VoxCore. You write scripts, modify game
systems, and register spells.

## Rules
- Always read existing code before modifying
- Follow the coding conventions in .claude/rules/coding-conventions.md
- Test by building after changes
```

### Agent Frontmatter Fields

| Field | Type | Purpose |
|-------|------|---------|
| `name` | string | How to reference this agent type |
| `description` | string | Shown when selecting agent types |
| `tools` | string[] | Which tools this agent can use (restricts access) |
| `model` | string | Model override (e.g., `sonnet` for cheaper agents) |
| `mode` | string | Permission mode override |

### Built-in Agent Types (6+)

| Type | Purpose | Tools Available |
|------|---------|----------------|
| `general-purpose` | Default — can do anything | All tools |
| `Explore` | Fast codebase exploration | Read-only tools (no Edit/Write) |
| `Plan` | Architecture planning | Read-only tools |
| `code-writer` | Implementation | All tools |
| `researcher` | Research and investigation | Read-only + MCP |
| Custom types | Your `.claude/agents/*.md` definitions | As specified |

### Fork Mode (Cache Optimization)

When the Agent tool spawns a subagent, it can use "fork mode" — the subagent's API prompt starts with the exact same bytes as the parent's prompt. This triggers Anthropic's prompt caching:

```
Parent prompt:  [system prompt][CLAUDE.md][conversation history]
                ↓ cache hit (identical prefix)
Subagent prompt: [system prompt][CLAUDE.md][agent-specific instructions]
```

The shared prefix is cached and doesn't re-incur token costs. This makes spawning 5 agents only marginally more expensive than 1.

**Source**: `tools/AgentTool/forkSubagent.ts`

## The Swarm System

The swarm system enables multiple Claude Code instances to work together as a team, with a leader coordinating workers.

### Swarm Backends

| Backend | How It Works | Pros | Cons |
|---------|-------------|------|------|
| **In-Process** | Workers run as Node.js child processes | Fastest, shared memory | RSS can explode (36.8GB incident led to 50-message cap) |
| **Tmux** | Workers run in tmux panes | Visual, debuggable | Requires tmux |
| **iTerm2** | Workers run in iTerm2 tabs | macOS native | macOS only |

### Team Lifecycle

```
TeamCreate → Team config + task list created
  → Agent spawns teammates (each gets a name)
  → TaskCreate → tasks added to shared list
  → TaskUpdate → tasks assigned to teammates
  → Teammates work independently, mark tasks complete
  → SendMessage → teammates communicate with leader
  → Leader monitors via TaskList
  → Shutdown requests → teammates exit gracefully
  → TeamDelete → cleanup
```

### Team Files

```
~/.claude/teams/{team-name}/config.json    — team members, roles
~/.claude/tasks/{team-name}/               — shared task list
```

### Messaging

Teammates communicate via a mailbox system:
- **DM**: `SendMessage` with `type: "message"` + recipient name
- **Broadcast**: `SendMessage` with `type: "broadcast"` (expensive — sends to ALL)
- **Shutdown**: `SendMessage` with `type: "shutdown_request"`

Messages are automatically delivered when the recipient's turn ends. No polling needed.

### 50-Message Cap

In-process teammates are hard-capped at 50 messages to prevent memory explosion. This was learned from a production incident where unbounded teammate messages caused 36.8GB RSS usage.

**Source**: `tasks/InProcessTeammateTask/` — message counter with hard limit.

## Practical Patterns

### Pattern 1: Research Swarm

Fan out 5 Explore agents to investigate different parts of a codebase simultaneously:

```
Agent 1: Search src/tools/ for registration patterns
Agent 2: Search src/hooks/ for event types
Agent 3: Search src/services/ for API patterns
Agent 4: Search src/utils/ for shared utilities
Agent 5: Search src/commands/ for command implementations
```

Each returns findings. You synthesize.

### Pattern 2: Implementation + QA

Two agents in parallel:
- Agent 1 (code-writer): Implements the feature
- Agent 2 (researcher): Reviews the implementation after Agent 1 commits

### Pattern 3: Conditional Skills for Domain Work

Create skills that only activate in their domain:

```
.claude/skills/
  build-loop.md     (paths: ["src/**/*.cpp", "src/**/*.h"])
  smartai-check.md  (paths: ["sql/**/*.sql"])
  apply-sql.md      (paths: ["sql/**/*.sql"])
  pre-ship.md       (paths: ["tools/publishable/**"])
```

Each skill is invisible when working in unrelated domains, saving prompt tokens.

## Cross-References

- [Skill System Overview](../skills/overview.md) — technical architecture
- [Agent/Coordinator Deep Dive](../agents/coordinator_overview.md) — how Agent tool works
- [Swarm Backends](../agents/swarm_backends.md) — Tmux, iTerm2, InProcess details
- [Fork Mode](../agents/fork_mode.md) — prompt cache sharing
