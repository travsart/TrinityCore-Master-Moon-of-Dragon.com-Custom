---
description: "permission system deep dive — 7 modes, 8 rule sources, allow deny glob patterns, YOLO classifier, managed enterprise settings, consecutive denial tracking"
---

# Guide: Permission System Deep Dive — Arcanum Wiki

> How Claude Code decides whether to allow, deny, or ask about every tool call.

## The Permission Question

Every time Claude wants to call a tool (Read a file, run a Bash command, edit code), the permission system evaluates: **should this be allowed automatically, denied, or should the user be asked?**

## The 7 Permission Modes

Users select a permission mode that sets the baseline behavior:

| Mode | Behavior | When to Use |
|------|----------|------------|
| `default` | Ask for most write operations | Normal usage |
| `acceptEdits` | Auto-approve file edits, ask for Bash | Trust code changes |
| `allowEdits` | Auto-approve file edits + safe Bash | Higher trust |
| `bypassPermissions` | Auto-approve everything | Full trust (agents) |
| `dontAsk` | Never ask, deny instead of asking | CI/CD pipelines |
| `plan` | Restrict to read-only tools | Planning mode |
| `auto` | YOLO classifier decides | "Just do it" mode |

## The 8 Rule Sources

Permission decisions come from 8 sources, evaluated in priority order:

```
1. Managed settings (enterprise admin — highest priority)
2. User allow/deny rules (~/.claude/settings.json)
3. Project allow/deny rules (.claude/settings.json)
4. Hook decisions (PreToolUse hooks)
5. Permission mode baseline
6. Tool-specific defaults
7. YOLO classifier (auto mode only)
8. User prompt (fallback — ask the human)
```

### The Critical Rule: allow Does NOT Override deny

If source #2 (user rules) says `deny` and source #4 (hook) says `allow`, the tool is DENIED. The `allow` from a hook cannot override a `deny` from rules. This is defense-in-depth.

```
deny from ANY source → tool is denied (period)
allow from ALL sources → tool is allowed
mixed → most restrictive wins (ask > allow, deny > ask)
```

**Source**: `hooks/toolPermission/` — the permission merger.

## Allow/Deny Rules

Rules are glob patterns that match tool calls:

```json
{
  "permissions": {
    "allow": [
      "Read:*",
      "Glob:*",
      "Grep:*",
      "Bash(git *)",
      "Bash(npm test)",
      "Edit:src/**/*.ts"
    ],
    "deny": [
      "Bash(rm -rf *)",
      "Write:*.env",
      "Edit:.claude/settings*"
    ]
  }
}
```

### Pattern Syntax

| Pattern | Matches |
|---------|---------|
| `Read:*` | All Read tool calls |
| `Bash(git *)` | Bash commands starting with `git ` |
| `Bash(npm test)` | Exact Bash command |
| `Edit:src/**/*.ts` | Edit calls on TypeScript files in src/ |
| `Write:*.env` | Write calls on .env files |
| `*` | Everything (nuclear option) |

## The YOLO Classifier (Auto Mode)

When permission mode is `auto`, Claude Code uses a classifier to decide what's safe without asking. This is the "just do it" experience.

### How It Works

1. Tool call comes in
2. Classifier evaluates the call against safety heuristics
3. If safe → auto-approve
4. If risky → prompt the user
5. **Consecutive denial tracking**: If the user denies N times in a row, YOLO backs off and starts prompting for everything

### What YOLO Auto-Approves

- Read operations (Read, Glob, Grep)
- File edits in the project directory
- Safe Bash commands (git status, ls, npm test)
- Agent spawning (Explore agents)

### What YOLO Always Asks About

- Bash commands with destructive patterns (rm, force push, DROP)
- File operations outside the project directory
- Network operations (curl to unknown hosts)
- Operations on sensitive files (.env, credentials, keys)

### Dangerous Pattern Stripping

The permission system actively strips dangerous patterns from Bash commands before evaluation. Known dangerous patterns include:
- `rm -rf /`
- `git push --force`
- `DROP TABLE`
- Commands with pipes to destructive operations

**Source**: `utils/permissions/` — pattern matching and stripping logic.

## Enterprise Managed Settings

Organizations can push `managed` settings that override everything:

```json
{
  "managedSettings": {
    "permissions": {
      "deny": [
        "Bash(curl *)",
        "Bash(wget *)",
        "Write:*.key",
        "Write:*.pem"
      ]
    },
    "allowedDomains": ["github.com", "*.company.com"],
    "blockedDomains": ["pastebin.com"]
  }
}
```

Managed settings are the HIGHEST priority — users cannot override them.

**Source**: `services/remoteManagedSettings/`

## Permission Flow Diagram

```
Tool call: Bash("git push origin main")
  │
  ├── Check managed deny rules → No match → continue
  ├── Check user deny rules → No match → continue
  ├── Check project deny rules → No match → continue
  │
  ├── Check managed allow rules → No match → continue
  ├── Check user allow rules → Match "Bash(git *)" → ALLOW
  │
  ├── Run PreToolUse hooks → Hook returns "allow" → ALLOW
  │
  ├── Permission mode check → "acceptEdits" → Bash not auto-approved → ASK
  │
  └── Result: ASK (most restrictive of ALLOW + ASK = ASK)
```

Wait, that's not right. Let me correct:

```
Tool call: Bash("git push origin main")
  │
  ├── 1. Managed deny? → No → continue
  ├── 2. User deny? → No → continue
  ├── 3. Project deny? → No → continue
  │
  ├── 4. User allow? → "Bash(git *)" matches → ALLOW candidate
  │
  ├── 5. PreToolUse hooks? → Hook says ALLOW → ALLOW candidate
  │
  ├── 6. Permission mode? → "default" → Bash requires ask → ASK candidate
  │
  └── Merge: ALLOW + ALLOW + ASK → Final: ASK
      (ASK is more restrictive than ALLOW, so ASK wins)
```

To get auto-approval, ALL sources must agree on ALLOW (or not have an opinion).

## Practical Tips

### 1. Set Up Allow Rules for Your Workflow

If you always approve git and build commands, add them to allow rules:

```json
{
  "permissions": {
    "allow": [
      "Bash(git *)",
      "Bash(gh *)",
      "Bash(ninja *)",
      "Bash(cmake *)",
      "Read:*",
      "Glob:*",
      "Grep:*"
    ]
  }
}
```

### 2. Use deny Rules as Safety Nets

Even in permissive modes, add deny rules for things that should never happen:

```json
{
  "permissions": {
    "deny": [
      "Bash(rm -rf /)*",
      "Write:*.env",
      "Write:*credentials*",
      "Bash(*--force*main*)"
    ]
  }
}
```

### 3. Understand Auto Mode's Denial Tracking

If you're in auto mode and deny a few times, YOLO stops auto-approving and starts asking for everything. This is by design — it's learning that you're being cautious. Approve a few safe operations to reset the counter.

### 4. Agents Inherit Permission Context

When you spawn a subagent, it inherits the parent's permission context. But the swarm leader acts as a permission bridge — worker agents ask the leader, not the user directly.

## Cross-References

- [Permissions System Overview](../permissions/overview.md) — full technical architecture
- [YOLO Classifier](../permissions/yolo_classifier.md) — auto-mode internals
- [Glob Patterns](../permissions/glob_patterns.md) — pattern matching details
- [Hooks & Permissions](../hooks/overview.md) — how hooks interact with permissions
