---
description: "rules system — .claude/rules/ files, conditional vs unconditional, paths frontmatter activation, recursive walk-up, priority order, token savings"
---

# Rules System -- Arcanum Wiki

## Overview

The `.claude/rules/` directory system provides a structured way to organize Claude Code instructions by topic. Rules files are markdown documents that are loaded alongside CLAUDE.md content and injected into the model's context. The system supports two modes: unconditional rules that are always loaded, and conditional rules that activate only when the model interacts with files matching specified glob patterns.

## How It Works

### Discovery

Rules are loaded from three locations via `processMdRules()` in `src/utils/claudemd.ts:697`:

1. **Managed rules**: `/etc/claude-code/.claude/rules/*.md` (enterprise policy)
2. **User rules**: `~/.claude/rules/*.md` (user-global rules)
3. **Project rules**: `<each-dir-in-walk>/.claude/rules/*.md` (project-specific)

The directory walk goes from filesystem root to CWD, with files closer to CWD having higher priority (model pays more attention to later-loaded content).

### Recursive Walk

`processMdRules()` recursively walks the rules directory:
- Follows symlinks with cycle detection via a `visitedDirs` set
- Processes all `.md` files found at any depth
- Each file goes through `processMemoryFile()` which:
  - Parses YAML frontmatter (extracting `paths:` patterns)
  - Strips block-level HTML comments
  - Resolves `@include` directives (max depth 5)
  - Returns `MemoryFileInfo` objects

### Conditional vs Unconditional Rules

Rules files can declare activation conditions via YAML frontmatter:

```yaml
---
paths: "src/**/*.cpp, src/**/*.h"
---
Your C++ coding conventions here...
```

The `conditionalRule` parameter in `processMdRules()` controls filtering:
- `conditionalRule: false` -- Returns files WITHOUT `paths:` frontmatter (unconditional, always loaded)
- `conditionalRule: true` -- Returns files WITH `paths:` frontmatter (conditional, loaded on demand)

Unconditional rules are loaded at session start and included in every turn's context.

### Conditional Rule Activation

Conditional rules are matched at tool use time via `processConditionedMdRules()` at line 1354. When the model reads, edits, or writes a file, the file path is tested against each conditional rule's `paths:` patterns using the `ignore` library (gitignore-style matching).

The activation flow:
1. Model invokes a file tool (Read, Edit, Write) targeting a specific path
2. `processConditionedMdRules()` is called with the file path
3. Each conditional rule's `paths:` patterns are compiled into an `ignore()` instance
4. The file's relative path (relative to the rule file's location) is tested against the patterns
5. Matching rules are injected into the model's context for that turn

Once activated, conditional rules are injected as system-reminder content alongside the tool result. The `paths:` pattern supports:
- Standard glob patterns: `*.ts`, `src/**/*.cpp`
- Comma-separated patterns: `"src/**/*.cpp, src/**/*.h"`
- Negation patterns: `!tests/**`

### Ordering

Within a rules directory, files are NOT explicitly sorted. They come in the order `readdir()` returns them, which is filesystem-dependent (typically alphabetical on most systems but not guaranteed).

However, the directory walk order provides a predictable priority hierarchy:
- Rules closer to the filesystem root load first (lower priority)
- Rules closer to CWD load last (higher priority)
- Within the same directory level, order is filesystem-dependent

### File Processing Pipeline

Each rules file goes through this pipeline:

```
File on disk
  -> readFileSync()
  -> parseYAMLFrontmatter()  (extract paths:, name:, description:, etc.)
  -> stripHtmlComments()     (remove <!-- ... --> blocks)
  -> resolveIncludes()       (process @path directives, max depth 5)
  -> MemoryFileInfo object   (path, content, type, frontmatter)
```

## Key Source Files

| File | Purpose |
|------|---------|
| `src/utils/claudemd.ts` | `processMdRules()`, `processConditionedMdRules()`, file processing |
| `src/utils/frontmatterParser.ts` | YAML frontmatter extraction |
| `src/utils/markdownConfigLoader.ts` | Loads markdown files from .claude/ subdirectories |

## Configuration

Rules are configured by creating `.md` files in `.claude/rules/` directories at project, user, or managed levels. No settings.json configuration is needed.

**Unconditional rule** (always loaded):
```markdown
# Session Start Protocol

Always read session_state.md before responding.
```

**Conditional rule** (loaded when matching files are touched):
```yaml
---
paths: "src/**/*.cpp, src/**/*.h"
---
# C++ Conventions

Use `nullptr` instead of `NULL`. Follow the project's naming conventions.
```

## Cross-References

- [CLAUDE.md Injection](claude_md_injection.md) -- How rules files are part of the CLAUDE.md loading pipeline
- [System Prompt](system_prompt.md) -- Where rules content appears in the final prompt
- [Conditional Activation (Skills)](../skills/conditional_activation.md) -- Similar `paths:` mechanism for skills

## Interesting Findings

**Rules and skills share the same conditional activation mechanism.** Both use `paths:` frontmatter with gitignore-style patterns and the `ignore` library for matching. The code paths are separate but the user-facing behavior is identical.

**HTML comments are a power feature.** Because `<!-- ... -->` blocks are stripped before the model sees the content, rules authors can include editorial notes, disabled sections, and documentation that is invisible to the model. For example: `<!-- DISABLED: This rule caused issues in session 150, review before re-enabling -->`.

**No explicit limit on rules files.** Unlike MEMORY.md (200 lines, 25KB) or CLAUDE.md (40K char warning), there is no enforced limit on the number or size of rules files. However, every rules file consumes context tokens on every turn, so the practical constraint is context window budget.

**Symlink cycle detection prevents infinite loops.** The recursive walk tracks visited directories (resolved via `realpath`) in a Set. If a symlink creates a cycle, the directory is skipped on the second visit. This is a quiet safety measure -- no warning is emitted.
