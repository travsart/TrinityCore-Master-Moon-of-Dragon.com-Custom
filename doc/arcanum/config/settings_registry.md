---
description: "settings registry — 60+ settings keys, 5 merge sources, priority order, array concatenation dedup, policy always wins, enableAllProjectMcpServers"
---

# Settings Registry
> Source: `utils/settings/`, `src/schemas/`
> Status: STUB — needs research

## What This Covers
All 70+ settings keys, their types, defaults, scopes, and effects. Where settings are read from (env vars, config files, CLI flags).

## Source Files to Read
- `utils/settings/` — settings loading, merging, validation
- `src/schemas/` — JSON schema definitions for settings files

## Key Questions
- Complete list of all settings keys with types and defaults
- Merge order: CLI flag > env var > local config > project config > user config?
- Which settings are hot-reloadable vs require restart?
- Hidden/undocumented settings not in `claude config`?
