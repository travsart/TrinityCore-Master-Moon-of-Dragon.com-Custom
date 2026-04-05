---
description: "permission glob patterns — file path matching, tool-specific allow deny rules, glob syntax for permissions, pattern precedence"
---

# Permission Glob Patterns -- Arcanum Wiki

## Overview

Permission rules in Claude Code use a format of `ToolName` or `ToolName(ruleContent)` where `ruleContent` supports glob patterns, command prefixes, and the `ignore` library's gitignore-style matching for file paths. The rule parser handles escaping, legacy tool name normalization, and MCP server-level matching.

## How It Works

### Rule String Format

Rules follow the format `"ToolName"` or `"ToolName(ruleContent)"`:

```typescript
permissionRuleValueFromString(ruleString: string): PermissionRuleValue
```

| Rule String | Parsed `toolName` | Parsed `ruleContent` |
|-------------|-------------------|---------------------|
| `Bash` | `Bash` | `undefined` (tool-level) |
| `Bash(npm install)` | `Bash` | `npm install` |
| `Bash(*)` | `Bash` | `undefined` (wildcard = tool-level) |
| `Read(//c/Users/**)` | `Read` | `//c/Users/**` |
| `mcp__server1` | `mcp__server1` | `undefined` |

### Escape Sequences

Parentheses in rule content must be escaped:

```typescript
escapeRuleContent("python -c \"print(1)\"")
// Returns: "python -c \"print\\(1\\)\""
```

### File Path Matching

File permission rules use gitignore-style glob patterns via the `ignore` library. Pattern resolution depends on rule source:

| Pattern | Root Resolution |
|---------|----------------|
| `//c/Users/**` | Absolute path (Windows drive converted from POSIX) |
| `~/.ssh/**` | Relative to `$HOME` |
| `/src/**` | Relative to the settings file's root directory |
| `*.log` | Matches anywhere (no root) |

The `matchingRuleForInput()` function groups rules by root path, creates an `ignore()` instance per group, and tests the file's relative path.

### Bash Command Matching

For Bash tools, `ruleContent` is matched against the command prefix. The tool's `checkPermissions()` extracts the first command word and compares:

- `Bash(git status)` -- matches commands starting with `git status`
- `Bash(git:*)` -- matches any command starting with `git`
- `Bash(npm *)` -- matches `npm install`, `npm test`, etc.

Compound commands (pipes, `&&`, `;`) are decomposed by tree-sitter parsing, and each subcommand is checked independently.

### MCP Server-Level Matching

Rules can target entire MCP servers:

```
mcp__server1          -> matches ALL tools from server1
mcp__server1__*       -> matches ALL tools from server1
mcp__server1__tool1   -> matches only tool1 from server1
```

### Legacy Tool Name Normalization

Old tool names are transparently mapped:

```typescript
const LEGACY_TOOL_NAME_ALIASES = {
  Task: 'Agent',
  KillShell: 'TaskStop',
  AgentOutputTool: 'TaskOutput',
  BashOutputTool: 'TaskOutput',
}
```

### Dangerous Pattern Detection

When checking if a Bash permission is "dangerous" (for auto mode stripping), five shapes are tested for each pattern:

```
"python"          -> exact match
"python:*"        -> prefix with colon-star
"python*"         -> wildcard suffix
"python *"        -> space-wildcard
"python -*"       -> flag pattern
```

## Key Source Files

| File | Purpose |
|------|---------|
| `src/utils/permissions/permissionRuleParser.ts` | Rule parsing, escaping, normalization |
| `src/utils/permissions/filesystem.ts` | File path matching with `ignore` library |
| `src/utils/permissions/permissions.ts` | `toolMatchesRule()`, MCP matching |
| `src/utils/permissions/dangerousPatterns.ts` | Pattern lists for auto mode |

## Cross-References

- [Permissions Overview](overview.md) -- Where rules fit in the pipeline
- [YOLO Classifier](yolo_classifier.md) -- How dangerous patterns are stripped
- [Rules System](../core/rules_system.md) -- Separate `paths:` mechanism for CLAUDE.md rules

## Interesting Findings

**All path comparisons normalize case.** On macOS and Windows, path matching is case-insensitive to prevent bypass via case manipulation.

**Windows-specific path attack detection.** `hasSuspiciousWindowsPathPattern()` checks for NTFS alternate data streams, 8.3 short names, long path prefixes, trailing dots/spaces, DOS device names, triple dots, and UNC paths. These checks run on ALL platforms because NTFS can be mounted on Linux/macOS via ntfs-3g.

**The `ignore` library is used for both file permissions and conditional rules/skills.** Three separate systems (file permission matching, `.claude/rules/` conditional activation, and skill `paths:` activation) all use the same gitignore-style matching library.
