---
description: "plugins service — plugin loading lifecycle, bundled plugins, plugin discovery, reload-plugins command, plugin isolation"
---

# Plugins Service -- Arcanum Wiki

## What Is This?

The Plugins service manages Claude Code's plugin ecosystem -- discovering, installing, loading, and executing plugins that extend Claude Code's capabilities. Plugins can add new tools, slash commands, system prompt sections, and hooks. The system supports both bundled (built-in) plugins and marketplace-sourced plugins.

## How It Works

### Plugin Management (`src/services/plugins/`)

- **`PluginInstallationManager.ts`** -- Handles plugin installation, updates, and removal. Manages the plugin registry on disk and tracks installed versions.
- **`pluginOperations.ts`** -- Core operations: install, uninstall, update, list. Handles dependency resolution and compatibility checks.
- **`pluginCliCommands.ts`** -- CLI command handlers for `/plugin install`, `/plugin list`, `/plugin remove`, etc.

### Plugin Runtime (`src/plugins/`)

- **`builtinPlugins.ts`** -- Registry of plugins bundled with Claude Code (shipped in the binary)
- **`bundled/`** -- Directory containing the actual bundled plugin implementations

### Plugin Loading

Plugins are loaded during startup via the plugin loading pipeline:
1. Scan installed plugins from the plugin registry
2. Load each plugin's manifest (name, version, capabilities)
3. Initialize plugin modules (register tools, commands, hooks)
4. Inject plugin-provided system prompt sections

### Plugin Capabilities

Plugins can provide:
- **Tools** -- Additional tools the model can call (registered in the tool list)
- **Slash commands** -- User-invocable commands (appear in `/help`)
- **System prompt sections** -- Instructions injected into the model's context
- **Hooks** -- Pre/post-sampling hooks for background processing
- **Agents** -- Custom agent definitions loaded via `loadPluginAgents()`

### Marketplace Integration

The plugin system supports named marketplaces. The official Anthropic marketplace (`OFFICIAL_MARKETPLACE_NAME`) is the primary source. Marketplace configuration is loaded via `loadKnownMarketplacesConfigSafe()`.

Plugin identification uses the format `pluginName@marketplaceName`.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/services/plugins/PluginInstallationManager.ts` | Installation lifecycle management |
| `src/services/plugins/pluginOperations.ts` | Install, uninstall, update operations |
| `src/services/plugins/pluginCliCommands.ts` | CLI command handlers |
| `src/plugins/builtinPlugins.ts` | Bundled plugin registry |
| `src/plugins/bundled/` | Bundled plugin implementations |
| `src/utils/plugins/loadPluginAgents.ts` | Agent loading from plugins |
| `src/utils/plugins/installedPluginsManager.ts` | Installed plugin tracking |
| `src/utils/plugins/marketplaceManager.ts` | Marketplace configuration |

## Configuration

- Plugin directory: within `~/.claude/` configuration
- Marketplace config: persisted locally, supports multiple named sources
- Per-plugin settings in the plugin manifest
- The system init message includes installed plugins (name, path, source)

## Interesting Findings

1. **Plugins inject into the system prompt**, meaning they can meaningfully change the model's behavior. The system init SDK message includes all installed plugins for transparency.

2. **The tip registry includes marketplace plugin suggestions** -- it checks what files exist in the project and suggests relevant plugins (e.g., Docker plugin when Dockerfiles are present).

3. **Plugin agents** are loaded alongside built-in and user-defined agents, making them first-class citizens in the agent system. They appear in the `AgentTool` listing.

4. **`isPluginInstalled()`** uses the `pluginName@marketplaceName` format to check installation state, supporting multiple marketplaces with potentially overlapping plugin names.
