---
description: "session management commands — /session list resume, /resume continue session, /share export, /export save, /rename, /branch fork, /clear reset, hidden aliases"
---

# Session Management Commands -- Arcanum Wiki

## Overview

Session management commands handle conversation lifecycle: starting new sessions, resuming previous ones, branching conversations, exporting transcripts, and renaming sessions. These are core workflow commands that most users interact with daily.

## Commands

### /session
- **Aliases**: `/remote`
- **Arguments**: None
- **What it does**: Displays the remote session URL and a QR code when Claude Code is running in remote mode (`claude --remote`). Generates a UTF-8 QR code from the session URL using the `qrcode` library. If not in remote mode, shows a warning message.
- **Feature gating**: Only enabled when `getIsRemoteMode()` returns true. Hidden from the command list when not in remote mode.
- **Key code**:
```typescript
const session = {
  type: 'local-jsx',
  name: 'session',
  aliases: ['remote'],
  description: 'Show remote session URL and QR code',
  isEnabled: () => getIsRemoteMode(),
  get isHidden() {
    return !getIsRemoteMode()
  },
}
```
- **Notes**: The QR code generation silently fails -- the URL is always shown as a fallback. Press ESC to dismiss the panel.

---

### /resume
- **Aliases**: `/continue`
- **Arguments**: `[conversation id or search term]`
- **What it does**: Resumes a previous conversation. Without arguments, shows an interactive picker (LogSelector) listing all past conversations for the current project. With arguments, it attempts resolution in this order:
  1. UUID match -- directly resumes the session by ID
  2. Direct file lookup -- handles sessions that enrichLogs missed (e.g., first message >16KB)
  3. Exact custom title match -- if custom titles are enabled, searches by title
  4. Multiple title matches -- shows error with count
  5. No match -- shows "session not found" error
- **Feature gating**: Always enabled. Custom title search requires `isCustomTitleEnabled()`.
- **Cross-project handling**: When a conversation is from a different directory, the command copies a `claude -r <id>` command to the clipboard instead of resuming directly. Same-repo worktrees can resume directly.
- **Key code**:
```typescript
const resume: Command = {
  type: 'local-jsx',
  name: 'resume',
  description: 'Resume a previous conversation',
  aliases: ['continue'],
  argumentHint: '[conversation id or search term]',
}
```
- **Notes**: The picker supports toggling between same-repo and all-projects views. Sessions are filtered to exclude sidechains and the current session. Also supports agentic session search via `agenticSessionSearch`.

---

### /backfill-sessions
- **Arguments**: None
- **What it does**: STUBBED OUT. The command exists in the codebase but is permanently disabled.
- **Feature gating**: `isEnabled: () => false`, `isHidden: true`
- **Notes**: This appears to have been a command for migrating or backfilling session data. It is dead code.

---

### /share
- **Arguments**: None
- **What it does**: STUBBED OUT. Permanently disabled.
- **Feature gating**: `isEnabled: () => false`, `isHidden: true`
- **Notes**: Was likely planned for sharing conversation transcripts. Dead code.

---

### /export
- **Arguments**: `[filename]`
- **What it does**: Exports the current conversation to a text file or clipboard. If a filename is provided as an argument, writes directly to that file (appending `.txt` if no extension). Without arguments, shows an interactive ExportDialog that lets the user choose between file and clipboard export.
- **Feature gating**: None -- always available.
- **Implementation details**:
  - Uses `renderMessagesToPlainText()` to convert conversation messages to plain text
  - Auto-generates filenames from the first user prompt (sanitized, max 50 chars) plus timestamp
  - Format: `claude-YYYY-MM-DD-HHMMSS-first-prompt-text.txt`
  - Files are written to the current working directory
- **Key code**:
```typescript
const exportCommand = {
  type: 'local-jsx',
  name: 'export',
  description: 'Export the current conversation to a file or clipboard',
  argumentHint: '[filename]',
}
```

---

### /rename
- **Arguments**: `[name]`
- **What it does**: Renames the current conversation with a custom title. This title appears in the `/resume` picker and session listings.
- **Feature gating**: None -- always available.
- **Execution**: Marked as `immediate: true`, meaning it executes instantly without waiting for a model response.
- **Key code**:
```typescript
const rename = {
  type: 'local-jsx',
  name: 'rename',
  description: 'Rename the current conversation',
  immediate: true,
  argumentHint: '[name]',
}
```

---

### /branch
- **Aliases**: `/fork` (only when the `FORK_SUBAGENT` feature flag is disabled)
- **Arguments**: `[name]`
- **What it does**: Creates a branch (fork) of the current conversation at the current point. This is a deep copy operation that:
  1. Reads the current transcript JSONL file
  2. Filters to main conversation messages (excludes sidechains)
  3. Copies all messages with a new session ID while preserving metadata (timestamps, git branch, etc.)
  4. Copies content-replacement entries to maintain prompt cache efficiency
  5. Saves a custom title with " (Branch)" suffix (auto-increments: "Branch 2", "Branch 3" on collision)
  6. Automatically resumes into the forked session
- **Feature gating**: None -- always available. The `/fork` alias is conditionally available based on the `FORK_SUBAGENT` feature flag.
- **Key code**:
```typescript
const branch = {
  type: 'local-jsx',
  name: 'branch',
  aliases: feature('FORK_SUBAGENT') ? [] : ['fork'],
  description: 'Create a branch of the current conversation at this point',
  argumentHint: '[name]',
}
```
- **Notes**: After branching, the command prints a hint: `To resume the original: claude -r <originalSessionId>`. The branch includes `forkedFrom` traceability metadata linking each entry back to its original message UUID.

---

### /clear
- **Aliases**: `/reset`, `/new`
- **Arguments**: None
- **What it does**: Clears the entire conversation history and frees up context. This starts a completely fresh conversation within the same session. Unlike `/compact`, this does not preserve a summary.
- **Feature gating**: Not available in non-interactive mode (`supportsNonInteractive: false`).
- **Key code**:
```typescript
const clear = {
  type: 'local',
  name: 'clear',
  description: 'Clear conversation history and free up context',
  aliases: ['reset', 'new'],
  supportsNonInteractive: false,
}
```
- **Notes**: Implementation is lazy-loaded. Session caches are cleared via `clearSessionCaches` and conversation state via `clearConversation`.

## Hidden/Undocumented Commands

- **/backfill-sessions** -- Exists as a stub, permanently disabled. Never appears in `/help`.
- **/share** -- Exists as a stub, permanently disabled. Never appears in `/help`.
- The `/fork` alias for `/branch` is conditionally hidden based on the `FORK_SUBAGENT` feature flag.
