---
description: "miscellaneous commands — /help /exit /copy /vim /voice /keybindings /feedback /release-notes /plan /tag /summary /env /bughunter /tasks /status /rewind /statusline"
---

# Miscellaneous Commands -- Arcanum Wiki

## Overview

This article covers the remaining commands that do not fit neatly into other groups: core REPL operations, workflow assistance, system information, and various specialized tools.

## Commands

### /help
- **Arguments**: None
- **What it does**: Shows help and lists all available commands. Renders a help screen that displays every command the user has access to (filtering out hidden and disabled commands), grouped logically with descriptions.
- **Feature gating**: None -- always available.
- **Key code**:
```typescript
const help = {
  type: 'local-jsx',
  name: 'help',
  description: 'Show help and available commands',
}
```

---

### /exit
- **Aliases**: `/quit`
- **Arguments**: None
- **What it does**: Exits the Claude Code REPL. Terminates the current session.
- **Feature gating**: None -- always available.
- **Execution**: `immediate: true`
- **Key code**:
```typescript
const exit = {
  type: 'local-jsx',
  name: 'exit',
  aliases: ['quit'],
  description: 'Exit the REPL',
  immediate: true,
}
```

---

### /copy
- **Arguments**: `[N]` (optional, Nth-latest response)
- **What it does**: Copies Claude's last response to the clipboard. With a numeric argument, copies the Nth-most-recent response (e.g., `/copy 2` copies the second-to-last response).
- **Feature gating**: None -- always available.
- **Key code**:
```typescript
const copy = {
  type: 'local-jsx',
  name: 'copy',
  description: "Copy Claude's last response to clipboard " +
    "(or /copy N for the Nth-latest)",
}
```

---

### /vim
- **Arguments**: None (toggle)
- **What it does**: Toggles between Vim and Normal editing modes for the input area. When Vim mode is enabled, the input field supports vi keybindings (hjkl navigation, modes, etc.).
- **Feature gating**: Not available in non-interactive mode.
- **Key code**:
```typescript
const command = {
  name: 'vim',
  description: 'Toggle between Vim and Normal editing modes',
  supportsNonInteractive: false,
  type: 'local',
}
```

---

### /voice
- **Arguments**: None (toggle)
- **What it does**: Toggles voice mode -- allows speaking to Claude instead of typing. Uses the device microphone for speech-to-text input.
- **Feature gating**: Triple-gated:
  1. `isVoiceGrowthBookEnabled()` must return true (enables the command)
  2. `isVoiceModeEnabled()` controls visibility (may be more permissive)
  3. Only for `claude-ai` users
  4. Not available in non-interactive mode
- **Key code**:
```typescript
const voice = {
  type: 'local',
  name: 'voice',
  description: 'Toggle voice mode',
  availability: ['claude-ai'],
  isEnabled: () => isVoiceGrowthBookEnabled(),
  get isHidden() {
    return !isVoiceModeEnabled()
  },
  supportsNonInteractive: false,
}
```

---

### /keybindings
- **Arguments**: None
- **What it does**: Opens or creates the keybindings configuration file. Allows users to customize keyboard shortcuts for Claude Code operations.
- **Feature gating**: Only enabled when `isKeybindingCustomizationEnabled()` returns true. Not available in non-interactive mode.
- **Key code**:
```typescript
const keybindings = {
  name: 'keybindings',
  description: 'Open or create your keybindings configuration file',
  isEnabled: () => isKeybindingCustomizationEnabled(),
  supportsNonInteractive: false,
  type: 'local',
}
```

---

### /feedback
- **Aliases**: `/bug`
- **Arguments**: `[report]`
- **What it does**: Submits feedback about Claude Code. Opens a feedback form or directly submits a bug report with the provided text.
- **Feature gating**: Disabled in many contexts:
  - Bedrock users
  - Vertex users
  - Foundry users
  - `DISABLE_FEEDBACK_COMMAND` or `DISABLE_BUG_COMMAND` env vars
  - Essential-traffic-only privacy mode
  - Anthropic employees (`USER_TYPE === 'ant'` -- they use internal channels)
  - When `allow_product_feedback` policy is not allowed
- **Key code**:
```typescript
const feedback = {
  aliases: ['bug'],
  type: 'local-jsx',
  name: 'feedback',
  description: 'Submit feedback about Claude Code',
  argumentHint: '[report]',
  isEnabled: () => !(
    isEnvTruthy(process.env.CLAUDE_CODE_USE_BEDROCK) ||
    isEnvTruthy(process.env.CLAUDE_CODE_USE_VERTEX) ||
    isEnvTruthy(process.env.CLAUDE_CODE_USE_FOUNDRY) ||
    isEnvTruthy(process.env.DISABLE_FEEDBACK_COMMAND) ||
    isEnvTruthy(process.env.DISABLE_BUG_COMMAND) ||
    isEssentialTrafficOnly() ||
    process.env.USER_TYPE === 'ant' ||
    !isPolicyAllowed('allow_product_feedback')
  ),
}
```

---

### /release-notes
- **Arguments**: None
- **What it does**: Displays the release notes for the current version of Claude Code. Shows what changed in the latest update.
- **Feature gating**: None -- always available. Supports non-interactive mode.
- **Key code**:
```typescript
const releaseNotes: Command = {
  description: 'View release notes',
  name: 'release-notes',
  type: 'local',
  supportsNonInteractive: true,
}
```

---

### /plan
- **Arguments**: `[open|<description>]`
- **What it does**: Enables plan mode or views the current session plan. Plan mode structures the conversation around a specific goal with tracked progress. The underlying "ultraplan" feature can:
  - Create structured plans with steps
  - Track completion status
  - Optionally teleport to a remote environment for complex multi-agent exploration (30-minute timeout)
  - Use Claude Code on the web (CCR) for deeper analysis

  The standalone ultraplan (`ultraplan.tsx`) implements the remote planning flow:
  1. Checks remote agent eligibility
  2. Reads a prompt template from a file
  3. Teleports to a cloud environment
  4. Polls for plan approval (30-min timeout)
  5. Archives the remote session when done
- **Feature gating**: Basic plan mode is always available. Ultraplan remote features require GrowthBook flags and remote session policy.
- **Key code**:
```typescript
const plan = {
  type: 'local-jsx',
  name: 'plan',
  description: 'Enable plan mode or view the current session plan',
  argumentHint: '[open|<description>]',
}
```

---

### /tag
- **Arguments**: `<tag-name>`
- **What it does**: Toggles a searchable tag on the current session. Tags make it easier to find specific sessions later. This is a labeling/categorization system.
- **Feature gating**: Only enabled for Anthropic employees (`USER_TYPE === 'ant'`).
- **Key code**:
```typescript
const tag = {
  type: 'local-jsx',
  name: 'tag',
  description: 'Toggle a searchable tag on the current session',
  isEnabled: () => process.env.USER_TYPE === 'ant',
  argumentHint: '<tag-name>',
}
```

---

### /summary
- **Arguments**: Unknown (stubbed)
- **What it does**: STUBBED OUT. Was likely a conversation summary generator.
- **Feature gating**: `isEnabled: () => false`, `isHidden: true`

---

### /env
- **Arguments**: Unknown (stubbed)
- **What it does**: STUBBED OUT. Was likely for displaying environment variables or configuration.
- **Feature gating**: `isEnabled: () => false`, `isHidden: true`

---

### /onboarding
- **Arguments**: Unknown (stubbed)
- **What it does**: STUBBED OUT. Was likely the initial user onboarding flow.
- **Feature gating**: `isEnabled: () => false`, `isHidden: true`

---

### /bughunter
- **Arguments**: Unknown (stubbed)
- **What it does**: STUBBED OUT. Was likely an automated bug-finding tool, possibly related to the ultrareview system (they share the same GrowthBook config key `tengu_review_bughunter_config`).
- **Feature gating**: `isEnabled: () => false`, `isHidden: true`

---

### /tasks
- **Aliases**: `/bashes`
- **Arguments**: None
- **What it does**: Lists and manages background tasks. Shows currently running background processes (Bash commands, agent operations) with their status, output, and controls for cancellation.
- **Feature gating**: None -- always available.
- **Key code**:
```typescript
const tasks = {
  type: 'local-jsx',
  name: 'tasks',
  aliases: ['bashes'],
  description: 'List and manage background tasks',
}
```
- **Notes**: The `/bashes` alias reveals this was originally focused on background Bash commands before being generalized to all task types.

---

### /status
- **Arguments**: None
- **What it does**: Shows comprehensive Claude Code status including:
  - Version information
  - Current model
  - Account/authentication status
  - API connectivity
  - Tool statuses (which tools are available and healthy)
  - MCP server status
- **Feature gating**: None -- always available.
- **Execution**: `immediate: true`
- **Key code**:
```typescript
const status = {
  type: 'local-jsx',
  name: 'status',
  description: 'Show Claude Code status including version, model, account, ' +
    'API connectivity, and tool statuses',
  immediate: true,
}
```

---

### /rewind
- **Aliases**: `/checkpoint`
- **Arguments**: None (interactive selector)
- **What it does**: Restores the code and/or conversation to a previous point. Opens a message selector that lets the user choose a point in the conversation history to rewind to. This can undo both conversation messages AND file changes made since that point.
- **Feature gating**: Not available in non-interactive mode.
- **Implementation**: Remarkably simple -- calls `context.openMessageSelector()` and returns a skip result.
- **Key code**:
```typescript
export async function call(
  _args: string,
  context: ToolUseContext,
): Promise<LocalCommandResult> {
  if (context.openMessageSelector) {
    context.openMessageSelector()
  }
  return { type: 'skip' }
}

const rewind = {
  description: 'Restore the code and/or conversation to a previous point',
  name: 'rewind',
  aliases: ['checkpoint'],
}
```
- **Notes**: The heavy lifting is done by `openMessageSelector` which handles the UI for picking a restore point and the actual rewind logic.

---

### /statusline
- **Arguments**: `[custom prompt]`
- **What it does**: Sets up Claude Code's status line UI. This is a `prompt` type command that creates a sub-agent (`statusline-setup` type) to configure the terminal status line by reading the user's shell PS1 configuration and creating a matching status display.
- **Allowed tools**: `Agent`, `Read(~/**)`, `Edit(~/.claude/settings.json)`
- **Key code**:
```typescript
const statusline = {
  type: 'prompt',
  description: "Set up Claude Code's status line UI",
  name: 'statusline',
  progressMessage: 'setting up statusLine',
  allowedTools: [AGENT_TOOL_NAME, 'Read(~/**)', 'Edit(~/.claude/settings.json)'],
  source: 'builtin',
  disableNonInteractive: true,
  async getPromptForCommand(args) {
    const prompt = args.trim() || 'Configure my statusLine from my shell PS1 configuration'
    return [{ type: 'text',
      text: `Create an ${AGENT_TOOL_NAME} with subagent_type "statusline-setup" and the prompt "${prompt}"`
    }]
  },
}
```

---

### /install
- **Arguments**: `[latest|stable|version]` (e.g., `1.0.34`)
- **What it does**: Installs or updates Claude Code via the native installer. Goes through several phases:
  1. **Checking** -- Verifies current installation state
  2. **Cleaning npm** -- Removes any npm-based installations
  3. **Installing** -- Downloads and installs the specified version
  4. **Setting up** -- Configures shell aliases and paths
  5. **Success/Error** -- Reports final status

  Has a `force` option and supports targeting specific versions.
- **Notes**: This is in `install.tsx`, a standalone React component file. It handles migration from npm-based installations to native installations.

## Hidden/Undocumented Commands

- **/summary** -- Stubbed out
- **/env** -- Stubbed out
- **/onboarding** -- Stubbed out
- **/bughunter** -- Stubbed out
- **/tag** -- Ant-only
- **/files** -- Ant-only (documented in navigation.md)
- **/statusline** -- Not hidden but not widely known
- **/install** -- Standalone `.tsx` file, may not appear in standard help
- The `/bashes` alias for `/tasks` is undocumented
- The `/checkpoint` alias for `/rewind` is undocumented
