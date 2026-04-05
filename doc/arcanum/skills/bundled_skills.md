---
description: "bundled skills — 17 built-in skills, commit review-pr pdf, skill source code, override by project skills, bundled skill list"
---

# Bundled Skills -- Arcanum Wiki

## Overview

Bundled skills are compiled into the Claude Code binary and registered at startup via `initBundledSkills()`. Unlike file-based skills, they cannot be disabled by users (except for built-in plugin skills which have a toggle UI). Some are feature-gated behind build flags and use dynamic `require()` to enable dead-code elimination.

## How It Works

### Complete Bundled Skill List

| Skill | Availability | Key Feature |
|-------|-------------|-------------|
| `update-config` | All users | Settings.json editor with hooks verification |
| `keybindings-help` | All (feature-gated) | Keybinding customization; `userInvocable: false` (model-only) |
| `verify` | Ant-only | Verify code with reference files extracted to disk |
| `debug` | All users | Session debug log reader; `disableModelInvocation: true` |
| `lorem-ipsum` | Ant-only | Token-counted filler text (up to 500K tokens) |
| `skillify` | Ant-only | Captures workflow as reusable SKILL.md |
| `remember` | Ant-only (auto-memory) | Reviews and proposes memory promotions |
| `simplify` | All users | 3-agent parallel code review (reuse, quality, efficiency) |
| `batch` | All users | Parallel worktree orchestration (5-30 agents); `disableModelInvocation: true` |
| `stuck` | Ant-only | Diagnoses frozen sessions, posts to Slack |
| `dream` | Feature-flagged (KAIROS/KAIROS_DREAM) | Memory consolidation |
| `hunter` | Feature-flagged (REVIEW_ARTIFACT) | Dynamic require |
| `loop` | Feature-flagged (AGENT_TRIGGERS) | Recurring prompt scheduler via cron |
| `schedule` | Feature-flagged (AGENT_TRIGGERS_REMOTE) | Remote agent trigger management |
| `claude-api` | Feature-flagged (BUILDING_CLAUDE_APPS) | Claude API/SDK reference with language detection |
| `claude-in-chrome` | Auto-detected (Chrome extension) | Browser automation via MCP tools |
| `run-skill-generator` | Feature-flagged (RUN_SKILL_GENERATOR) | Dynamic require |

### File Extraction Security

Bundled skills with a `files` property have reference files extracted to disk on first invocation. The extraction uses `O_EXCL | O_NOFOLLOW` flags and `0o600` permissions. Path traversal is prevented by validating that normalized relative paths do not contain `..` components or absolute paths.

The extraction directory uses a process-specific nonce path under `getBundledSkillsRoot()`, and the extracted directory path is prepended to the skill prompt as `"Base directory for this skill: <dir>"`.

### Feature-Flagged Registration

Some skills use dynamic `require()` behind feature flags to enable dead-code elimination:

```typescript
if (feature('KAIROS') || feature('KAIROS_DREAM')) {
  const { registerDreamSkill } = require('./dream.js')
  registerDreamSkill()
}
```

### Built-in Plugin Skills vs Bundled Skills

| Property | Bundled Skills | Built-in Plugin Skills |
|----------|---------------|----------------------|
| Visibility | Always loaded | Appear in `/plugin` UI |
| Toggle | Cannot disable | User can enable/disable |
| Source tag | `source: 'bundled'` | `source: 'bundled'` (same!) |
| Components | Single skill | Multiple skills + hooks + MCP servers |
| Default | Always on | Configurable via `defaultEnabled` |

Despite the source being `'bundled'` for both (to keep analytics consistent), the user-toggleable aspect is tracked on `LoadedPlugin.isBuiltin`.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/skills/bundled/index.ts` | Init function registering all bundled skills |
| `src/skills/bundledSkills.ts` | Registry, file extraction, security validation |
| `src/plugins/builtinPlugins.ts` | Built-in plugin skill registration |

## Configuration

Bundled skills cannot be individually configured. Feature-flagged skills activate based on build flags and GrowthBook gates.

## Cross-References

- [Skills Overview](overview.md) -- Full architecture
- [Conditional Activation](conditional_activation.md) -- How skills interact with paths

## Interesting Findings

**`disableModelInvocation` vs `userInvocable` are independent.** `batch` and `debug` set `disableModelInvocation: true` (only user can invoke via `/batch`), while `keybindings-help` sets `userInvocable: false` (hidden from autocomplete but model can proactively invoke). These are opposite visibility strategies.

**Bundled skills can be overridden.** Project-scoped agents and skills with the same name as a built-in will replace the built-in in the resolution chain, because later sources override earlier ones. However, bundled skills always keep full descriptions in listings (non-bundled descriptions get truncated first).
