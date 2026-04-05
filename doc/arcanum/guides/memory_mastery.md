---
description: "memory system mastery — Sonnet selector, frontmatter description, 200 file cap, 5 per turn, MEMORY.md 200 line limit, AutoDream, topic file organization"
---

# Guide: Mastering the Memory System — Arcanum Wiki

> How Claude Code's memory actually works internally, and how to exploit it for maximum recall.

## The Memory Architecture

Claude Code has three memory layers:

```
┌────────────────────────────────────────────┐
│ Layer 1: MEMORY.md (always loaded)         │
│   - 200 line hard cap (lines after truncated)
│   - 25KB hard cap                          │
│   - Loaded EVERY turn, no selection needed │
└────────────────────────────────────────────┘
         │
         ▼
┌────────────────────────────────────────────┐
│ Layer 2: Topic Files (selector picks 5)    │
│   - Up to 200 files tracked by mtime      │
│   - Sonnet side-query selects 5 per turn   │
│   - Selector only sees filename + desc     │
│   - Content loaded ONLY for selected 5     │
└────────────────────────────────────────────┘
         │
         ▼
┌────────────────────────────────────────────┐
│ Layer 3: AutoDream (background)            │
│   - Fires after 24h + 5 sessions          │
│   - 4-phase: Orient→Gather→Consolidate→Prune
│   - Gated by tengu_onyx_plover flag       │
└────────────────────────────────────────────┘
```

## How the Selector Works (The Critical Detail)

**Source**: `utils/memory/memorySelector.ts`

When Claude Code starts a turn, it runs a Sonnet side-query. This query receives:
1. Your current message
2. A list of ALL memory filenames
3. The `description` frontmatter from each file (if present)

It does NOT receive:
- The actual content of any file
- Previous conversation history
- Tool results

The selector returns up to 5 filenames. Only those 5 files are read and injected into context.

### Implications

- **Filename matters**: A file named `db-schema-notes.md` is more discoverable than `notes.md`
- **Description is critical**: Without a `description` frontmatter field, the selector only has the filename to go on
- **5 file limit**: If you have 30 topic files, 25 are invisible each turn. Design for this.
- **200 file cap**: Files beyond 200 (oldest by mtime) are completely invisible. Touch files you want to keep visible.

## Writing Effective Frontmatter

The `description` field is your one chance to tell the selector what's inside:

```yaml
---
description: "DB schema reference — column names, table relationships, verified types for world/hotfixes/auth/characters/roleplay databases. Use when writing SQL or checking columns."
---
```

**Good patterns**:
- Include the domain keywords (e.g., "SQL", "database", "spells")
- Include action keywords (e.g., "Use when writing...", "Reference for...")
- Be specific about content type (e.g., "column names", "table relationships")

**Bad patterns**:
- `description: "Notes"` — useless, tells selector nothing
- `description: "Various stuff I learned"` — vague, won't match queries
- No frontmatter at all — selector only has filename

## MEMORY.md Strategy

MEMORY.md is your most valuable file — it's loaded EVERY turn. Use it for:

1. **Routing table**: Point to topic files by domain (like we do with the Session Routing table)
2. **Critical facts**: Things that apply to ALL sessions (user preferences, project identity)
3. **Active systems**: Current status of in-progress work
4. **Behavioral directives**: Rules that must always apply

Keep it under 180 lines (200 is the hard cap, leave buffer). Move details to topic files.

## Topic File Organization

### The Hub-and-Spoke Model

```
MEMORY.md (hub — always loaded, has links to spokes)
  ├── db-schema-notes.md (spoke — loaded when DB work detected)
  ├── spell-audit.md (spoke — loaded when spell work detected)
  ├── build-environment.md (spoke — loaded when build questions arise)
  └── case-status.md (spoke — loaded when legal work detected)
```

### Naming Conventions

The filename IS metadata. Use descriptive, keyword-rich names:

| Good | Bad |
|------|-----|
| `db-schema-notes.md` | `notes.md` |
| `spell-audit.md` | `audit.md` |
| `case-evidence-index-part1.md` | `evidence.md` |
| `addon-building-checklist.md` | `checklist.md` |

### When to Split Files

Split when a file exceeds ~300 lines or covers 2+ distinct topics. The selector can only pick 5 files — if one file covers everything, you're wasting slots on irrelevant content.

Split example: `case-evidence.md` (600 lines) → `case-evidence-index-part1.md` (folders 01-06) + `case-evidence-index-part2.md` (folders 07-15) + `case-evidence-index-part3.md` (special items)

## AutoDream Consolidation

AutoDream is a background process that runs when:
- At least 24 hours have passed since last consolidation
- At least 5 sessions have occurred
- Feature flag `tengu_onyx_plover` is enabled

It executes 4 phases:
1. **Orient**: Read all memory files to understand current state
2. **Gather**: Extract important information from recent sessions
3. **Consolidate**: Merge new information into existing memory files
4. **Prune**: Remove outdated or redundant information

**Lock file**: `~/.claude/memory/dream.lock` — its mtime is the `lastConsolidatedAt` timestamp.

You generally don't need to worry about AutoDream — it runs automatically. But knowing it exists explains why memory files sometimes get updated between sessions.

## Practical Tips

### 1. Keep a Topic Index

Maintain a `topic-index.md` file that lists every memory file with keywords. This helps both YOU and the selector find things:

```markdown
| Topic | File | Keywords |
|-------|------|----------|
| Build | build-environment.md | MSVC, cmake, ninja |
| DB Schema | db-schema-notes.md | columns, tables, SQL |
```

### 2. Touch Important Files

The 200-file cap is sorted by mtime. If you have 200+ files, old ones become invisible. Periodically `touch` files you want to keep accessible:

```bash
touch memory/important-file.md
```

### 3. Use /memory-audit Regularly

The memory audit skill checks for:
- MEMORY.md line count vs 200 limit
- Orphan files (not linked from any index)
- Broken links
- Stale files
- Large files that should be split

### 4. Don't Duplicate Between MEMORY.md and Topic Files

MEMORY.md should POINT to topic files, not repeat their content. Every duplicated line wastes tokens in your always-loaded file.

### 5. Corrections Must Update Source

If Claude states something from memory that turns out wrong, you MUST update the memory file. The memory system has no "expiration" — wrong information persists until manually corrected.

## Memory File Limits Summary

| Limit | Value | Source |
|-------|-------|--------|
| MEMORY.md max lines | 200 | Hard cap, lines after silently truncated |
| MEMORY.md max size | 25KB | Hard cap |
| Total tracked files | 200 | Oldest by mtime invisible beyond |
| Files loaded per turn | 5 | Sonnet selector picks |
| Post-compact restore budget | 50K tokens | Across up to 5 files |
| Per-file restore cap | 5K tokens | Single file during restore |

## Cross-References

- [Memory Pipeline Deep Dive](../core/memory_overview.md) — full technical architecture
- [Memory Selector](../core/memory_selector.md) — the Sonnet side-query algorithm
- [AutoDream](../core/autodream.md) — background consolidation details
- [Compaction & Memory](../core/compaction_overview.md) — what survives compaction
