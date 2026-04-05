---
description: "conditional skill activation — paths frontmatter, glob matching, touched file triggers, performance optimization, VoxCore 13 conditional skills"
---

# Conditional Skill Activation -- Arcanum Wiki

## Overview

Skills can declare `paths` in their YAML frontmatter to restrict when they become active. These conditional skills remain dormant and invisible to the model until file operations match their glob patterns. Once activated, a conditional skill stays active for the rest of the session. This is the same mechanism used by `.claude/rules/` files for conditional rule loading.

## How It Works

### Frontmatter Declaration

```yaml
---
name: cpp-review
description: C++ code review specialist
paths: "src/**/*.cpp, src/**/*.h"
---
```

Patterns use gitignore-style matching via the `ignore` library. The `parseSkillPaths()` function strips trailing `/**` suffixes (the library already matches recursively) and treats all-`**` patterns as equivalent to no filter.

### Loading Phase Separation

During initial loading, skills with non-trivial `paths` patterns are separated from unconditional skills:

```typescript
if (skill.paths && skill.paths.length > 0 && !activatedConditionalSkillNames.has(skill.name)) {
  newConditionalSkills.push(skill)  // Stored but NOT returned
} else {
  unconditionalSkills.push(skill)   // Included in listings
}
```

Conditional skills are stored in a `conditionalSkills` Map but are invisible to the model until activated.

### Activation Trigger

`activateConditionalSkillsForPaths()` is called when files are touched (Read, Edit, Write). It matches file paths (relative to CWD) against each conditional skill's patterns:

```typescript
const skillIgnore = ignore().add(skill.paths)
if (skillIgnore.ignores(relativePath)) {
  dynamicSkills.set(name, skill)
  conditionalSkills.delete(name)
  activatedConditionalSkillNames.add(name)
}
```

Once activated, the skill moves from `conditionalSkills` to `dynamicSkills` and stays active permanently. The `activatedConditionalSkillNames` Set survives cache clears.

### One-Way Activation

Activation is irreversible within a session. There is no deactivation mechanism -- once a conditional skill matches, it remains available even if no further matching files are touched. This prevents confusing the model with disappearing capabilities.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/skills/loadSkillsDir.ts` | `activateConditionalSkillsForPaths()`, `parseSkillPaths()` |

## Configuration

Add `paths:` to any skill's YAML frontmatter. Patterns support:
- Standard globs: `*.ts`, `src/**/*.cpp`
- Comma-separated: `"src/**/*.cpp, src/**/*.h"`
- Negation: `!tests/**`

## Cross-References

- [Skills Overview](overview.md) -- Full architecture
- [Discovery](discovery.md) -- Dynamic directory discovery
- [Rules System](../core/rules_system.md) -- Same `paths:` mechanism for rules files

## Interesting Findings

**Three systems share the same matching.** Conditional skills, conditional rules (`.claude/rules/` with `paths:` frontmatter), and file permission matching all use gitignore-style patterns via the `ignore` library.

**VoxCore uses this for 13 conditional skills.** In session 224, 13 VoxCore skills were made conditional to reduce system prompt bloat when working on unrelated subsystems.
