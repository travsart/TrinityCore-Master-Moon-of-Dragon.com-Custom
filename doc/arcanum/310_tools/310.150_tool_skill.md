---
description: "SkillTool — slash command execution, skill loading, forked sub-agent context, fully qualified names, args parameter, .claude/commands/ discovery, bundled skills"
title: "SkillTool -- Arcanum Wiki"
tags: [tools, slash-command, skill-loading, forked-sub-agent, fully-qualified, args-parameter, claudecommands-discovery, bundled-skills]
---

# SkillTool -- Arcanum Wiki

## Purpose

SkillTool executes slash commands (skills) within the conversation context. Skills are custom prompts defined in `.claude/commands/`, bundled with Claude Code, loaded from MCP servers, or installed from the marketplace. The tool runs skills in a forked sub-agent context with its own token budget.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `skill` | string | Yes | Skill name (e.g., "commit", "review-pr", "pdf") or fully qualified name ("ms-office-suite:pdf") |
| `args` | string | No | Optional arguments passed to the skill |

## Execution Flow

1. **Command lookup**: Searches all available commands (local, bundled, MCP skills) via `getAllCommands()`. Tries the exact name first, then attempts with common prefixes.
2. **Permission check**: Checks allow/deny rules for the skill name. Built-in skills, bundled skills, and official marketplace skills have relaxed permission handling.
3. **Forked execution**: Creates a unique agent ID, builds a forked sub-agent context via `prepareForkedCommandContext()`, and runs the skill prompt through `runAgent()`.
4. **Result extraction**: Extracts the agent's result text, cleans up invoked skills tracking.

## Key Implementation Details

### Command Sources
Skills can come from multiple sources:
- **Local**: `.claude/commands/` directory
- **Bundled**: Built into Claude Code (e.g., `/commit`, `/review-pr`)
- **MCP**: Skills provided by MCP servers (filtered to `loadedFrom === 'mcp'` to exclude plain MCP prompts)
- **Marketplace**: Installed via plugin system

The `getAllCommands()` function merges all sources, deduplicating by name.

### Telemetry Classification
For analytics, skill invocations are classified by source:
- Built-in or bundled commands: logged with their actual name
- Official marketplace skills: logged with their actual name
- Custom skills: logged as "custom" (privacy protection)

The `was_discovered` field tracks whether ToolSearch helped the model find this skill (for remote skill search feature).

### Remote Skill Search
Behind the `EXPERIMENTAL_SKILL_SEARCH` feature flag, skills can be discovered and loaded from a remote registry at runtime. The remote skill modules are conditionally imported to enable dead code elimination.

### Model Override
Skills can specify a model override in their frontmatter via `resolveSkillModelOverride()`. This allows specific skills to use a cheaper or more capable model than the session default.

## Limits and Constraints

| Limit | Value |
|-------|-------|
| maxResultSizeChars | 100,000 |
| Aliases | None |
| Deferred | Context-dependent |

## Permission Requirements

Per-skill allow/deny rules. Built-in commands have relaxed permissions. The permission content key is the skill name.

## Interesting Findings

1. Before MCP skill filtering was added, the model could invoke MCP prompts via SkillTool by guessing the `mcp__server__prompt` name -- they weren't discoverable but were technically reachable. The `loadedFrom === 'mcp'` filter closed this hole.

2. SkillTool records usage via `recordSkillUsage()` for suggestion tracking -- this feeds into the skill recommendation system that suggests relevant skills based on past usage.

3. The `clearInvokedSkillsForAgent()` cleanup at agent completion prevents skill invocation tracking from accumulating across agent lifetimes.
