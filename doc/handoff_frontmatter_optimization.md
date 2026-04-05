# Frontmatter Optimization — Split Between 2 Tabs

**Goal**: Keyword-pack the `description` frontmatter in all 158 arcanum .md files so Claude Code's memory selector (which ONLY reads filenames + `description` field) can find the right doc when working on related topics.

**Why this matters**: The Sonnet memory selector never reads file content during selection — it sees only the filename and the YAML `description` field. A doc about the YOLO classifier with `description: "YOLO classifier overview"` will be missed when the session involves "permission auto-accept dangerous patterns". But `description: "YOLO classifier — permission auto-accept, dangerous pattern detection, consecutive denial tracking, auto mode fallback"` gets found.

**Format**: Every .md file should have YAML frontmatter at the top:
```yaml
---
description: "keyword-packed description — key terms, related concepts, what you'd search for to find this doc"
---
```

**Rules**:
1. Keep descriptions under 200 chars (selector truncates longer)
2. Front-load the most important keywords
3. Include: system names, function names, config keys, error messages, concepts
4. Use em dashes to separate the title from keywords
5. If the file already has good frontmatter, skip it
6. If the file has a `> Source:` or `> Status:` line, use that info in the description
7. Don't change any content — ONLY add/update the `description` frontmatter field

**Example transforms**:
```
BEFORE: description: "Hook system overview"
AFTER:  description: "hooks overview — PreToolUse PostToolUse Notification Stop events, shell prompt agent http command types, 4-way permission race, hook allow deny"

BEFORE: (no frontmatter)
AFTER:  ---
        description: "bash execution internals — shell init profile loading, working directory persistence, timeout handling, background commands, Windows MSYS2 Git Bash"
        ---
```

---

## Tab 1 — THIS TAB (79 files)

Folders: `tools/`, `services/`, `hidden/`, `agents/`, `permissions/`, `skills/`, `api/`, `internals/`, `query/`, `limits/`, `sandbox/`, `telemetry/`

```
Read and update frontmatter for all .md files in these doc/arcanum/ subfolders:

  tools/       (21 files) — one per built-in tool, pipeline overview
  services/    (14 files) — compact, memories, oauth, plugins, etc.
  hidden/      (11 files) — buddy, voice, computer use, ultraplan, etc.
  agents/       (7 files) — coordinator, fork, swarm, teams
  permissions/  (6 files) — modes, yolo, rules, evaluation order
  skills/       (5 files) — loading, bundled, conditional, discovery
  api/          (3 files) — overview, context assembly, messages
  internals/    (3 files) — boot sequence, state, entry points
  query/        (3 files) — query loop, streaming, tool dispatch
  limits/       (2 files) — rate limiting, usage tracking
  sandbox/      (2 files) — sandbox overview, bash execution
  telemetry/    (2 files) — analytics events, cost tracking

Total: 79 files.

For each file:
1. Read it
2. Check if it has a `description:` in YAML frontmatter
3. If missing or generic, add/update with keyword-packed description
4. Use the Edit tool to add frontmatter (don't rewrite the whole file)

Work in batches — read 5-10 files, update them, move to next batch.
When done, commit: "docs: keyword-pack arcanum frontmatter (tab 1 — 79 files)"
```

---

## Tab 2 — OTHER TAB (79 files)

Folders: `guides/`, `core/`, `commands/`, `hooks/`, `mcp/`, `bridge/`, `ui/`, `config/`, `networking/`, `git/`, `plugins/`, `source/`, `index.md`

```
Read and update frontmatter for all .md files in these doc/arcanum/ subfolders:

  guides/      (15 files) — practical how-to guides for power users
  core/        (14 files) — compaction, memory, context, rules, system prompt
  commands/    (12 files) — slash command catalog by category
  hooks/        (8 files) — events, pipeline, permission race, types
  mcp/          (7 files) — connection lifecycle, transports, oauth, channels
  bridge/       (5 files) — IDE integration, websocket, session lifecycle
  ui/           (5 files) — Ink renderer, components, layout
  config/       (3 files) — settings registry, migrations, schema
  networking/   (3 files) — remote execution, server mode, proxy
  git/          (2 files) — git operations, github integration
  plugins/      (2 files) — plugin system, DXT extensions
  source/       (2 files) — root files analysis, constants/prompts
  index.md      (1 file)  — main arcanum index

Total: 79 files.

For each file:
1. Read it
2. Check if it has a `description:` in YAML frontmatter
3. If missing or generic, add/update with keyword-packed description
4. Use the Edit tool to add frontmatter (don't rewrite the whole file)

Work in batches — read 5-10 files, update them, move to next batch.
When done, commit: "docs: keyword-pack arcanum frontmatter (tab 2 — 79 files)"
```
