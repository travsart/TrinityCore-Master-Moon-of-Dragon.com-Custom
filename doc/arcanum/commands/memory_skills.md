---
description: "memory skills commands — /memory /skills /agents /hooks /config /mcp /plugin /reload-plugins /init /init-verifiers, configuration management"
---

# Memory, Skills & Plugin Commands -- Arcanum Wiki

## Overview

These commands manage Claude Code's extensibility layer: memory files (persistent instructions), skills (reusable capabilities), agents (configurable sub-agents), hooks (event-driven scripts), MCP servers (tool providers), and plugins (third-party extensions from the marketplace).

## Commands

### /memory
- **Arguments**: None
- **What it does**: Opens an editor for Claude memory files. Memory files are markdown files (typically `CLAUDE.md` at project root, or files in `.claude/` directories) that provide persistent instructions loaded into every session. The command lets users view and edit these files.
- **Feature gating**: None -- always available.
- **Key code**:
```typescript
const memory: Command = {
  type: 'local-jsx',
  name: 'memory',
  description: 'Edit Claude memory files',
}
```
- **Notes**: Memory files are hierarchical -- there are user-level (`~/.claude/CLAUDE.md`), project-level (`CLAUDE.md`), and directory-level (`.claude/` files) memories.

---

### /skills
- **Arguments**: None
- **What it does**: Lists all available skills in the current context. Skills are reusable, composable capabilities defined as markdown files with frontmatter. They can be invoked via slash commands and provide specialized domain knowledge or workflows.
- **Feature gating**: None -- always available.
- **Key code**:
```typescript
const skills = {
  type: 'local-jsx',
  name: 'skills',
  description: 'List available skills',
}
```
- **Notes**: Skills can come from the project `.claude/skills/` directory, user-level skills, or installed plugins.

---

### /agents
- **Arguments**: None
- **What it does**: Manages agent configurations. Agents are pre-configured sub-agent definitions that specify a model, system prompt, allowed tools, and other parameters. This command lets users view, create, and modify agent configurations.
- **Feature gating**: None -- always available.
- **Key code**:
```typescript
const agents = {
  type: 'local-jsx',
  name: 'agents',
  description: 'Manage agent configurations',
}
```

---

### /hooks
- **Arguments**: None
- **What it does**: Views hook configurations for tool events. Hooks are user-defined scripts that run before or after specific tool events (e.g., before a Bash command, after a file edit). The command displays all configured hooks and their trigger conditions.
- **Feature gating**: None -- always available.
- **Execution**: `immediate: true`
- **Key code**:
```typescript
const hooks = {
  type: 'local-jsx',
  name: 'hooks',
  description: 'View hook configurations for tool events',
  immediate: true,
}
```

---

### /config
- **Aliases**: `/settings`
- **Arguments**: None
- **What it does**: Opens the centralized configuration panel. This is the main settings UI that covers output style, model preferences, and other global options. (Also documented in the Model Config article.)
- **Feature gating**: None -- always available.

---

### /mcp
- **Arguments**: `[enable|disable [server-name]]`
- **What it does**: Manages MCP (Model Context Protocol) servers. MCP servers are external processes that provide additional tools to Claude Code. This command lets users:
  - List all configured MCP servers and their status
  - Enable/disable specific servers
  - View server health and available tools

  With arguments, directly enables or disables a named server. Without arguments, shows an interactive management panel.
- **Feature gating**: None -- always available.
- **Execution**: `immediate: true`
- **Key code**:
```typescript
const mcp = {
  type: 'local-jsx',
  name: 'mcp',
  description: 'Manage MCP servers',
  immediate: true,
  argumentHint: '[enable|disable [server-name]]',
}
```

---

### /plugin
- **Aliases**: `/plugins`, `/marketplace`
- **Arguments**: None (interactive)
- **What it does**: Opens the plugin management interface. This is a comprehensive system with multiple sub-screens:
  - **ManagePlugins** -- View/uninstall installed plugins
  - **BrowseMarketplace** -- Browse available plugins
  - **DiscoverPlugins** -- Discovery/recommendation interface
  - **AddMarketplace** -- Add custom marketplace sources
  - **ManageMarketplaces** -- Manage marketplace sources
  - **PluginSettings** -- Per-plugin configuration
  - **ValidatePlugin** -- Plugin validation
  - **PluginTrustWarning** -- Security warnings for untrusted plugins

  Plugins can provide commands, skills, MCP servers, and other extensions.
- **Feature gating**: None -- always available.
- **Execution**: `immediate: true`
- **Key code**:
```typescript
const plugin = {
  type: 'local-jsx',
  name: 'plugin',
  aliases: ['plugins', 'marketplace'],
  description: 'Manage Claude Code plugins',
  immediate: true,
}
```
- **Notes**: This is one of the largest commands by code volume. The plugin directory contains 19 files totaling over 800KB of code covering the full plugin lifecycle. The marketplace supports multiple sources and trust verification.

---

### /reload-plugins
- **Arguments**: None
- **What it does**: Activates pending plugin changes in the current session. When plugins are installed, updated, or removed, the changes may not take effect until this command is run (or the session is restarted). This performs a "Layer-3 refresh" -- hot-reloading plugin state.
- **Feature gating**: Not available in non-interactive mode. SDK callers use `query.reloadPlugins()` instead.
- **Key code**:
```typescript
const reloadPlugins = {
  type: 'local',
  name: 'reload-plugins',
  description: 'Activate pending plugin changes in the current session',
  supportsNonInteractive: false,
}
```
- **Notes**: The command comment explains that SDK callers get structured data back (commands, agents, plugins, mcpServers) for UI updates, while the slash command version is for interactive use.

---

### /init
- **Arguments**: None (interactive)
- **What it does**: Sets up a minimal CLAUDE.md (and optionally skills and hooks) for the current repository. The behavior varies by feature flag:

  **Old init** (default): Analyzes the codebase and creates CLAUDE.md with:
  - Build/lint/test commands
  - High-level architecture
  - Content from Cursor rules, Copilot instructions, and README

  **New init** (feature-gated): Interactive setup that asks:
  - Which CLAUDE.md to create (project, personal `.local.md`, or both)
  - Whether to also set up skills and hooks
  - More structured, question-driven approach
- **Feature gating**: The new init flow is behind a feature flag (checked in the implementation). Can be disabled.
- **Notes**: This is a standalone `.ts` file, not a directory. It calls `maybeMarkProjectOnboardingComplete()` when done.

---

### /init-verifiers
- **Arguments**: None
- **What it does**: Creates verifier skill(s) for automated verification of code changes. This is a `prompt` type command that instructs Claude to:
  1. Auto-detect project types and stacks across subdirectories
  2. Create appropriate verifiers based on application type:
     - Web apps: Playwright-based verifier
     - CLI tools: Tmux-based verifier
     - API services: HTTP-based verifier
  3. Explicitly excludes unit tests and typechecking (handled by standard build/test)
- **Notes**: Uses `TodoWrite` tool for progress tracking. Creates files in `.claude/skills/`.

## Hidden/Undocumented Commands

- **/init** -- Not a subdirectory command; registered differently as a standalone file.
- **/init-verifiers** -- Also a standalone file; may not appear in standard `/help`.
- **/reload-plugins** -- Visible but primarily an internal mechanism.
