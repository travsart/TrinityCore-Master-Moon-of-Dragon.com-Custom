---
description: "tool pipeline overview — buildTool factory, Tool.ts, ToolDef interface, tool registration, assembleToolPool, concurrent batching, ToolSearch deferred loading, tool result persistence"
---

# Tool Pipeline Overview -- Arcanum Wiki

## How Tools Are Built

Every tool in Claude Code is constructed via the `buildTool()` factory function defined in `src/Tool.ts:783`. This function accepts a partial tool definition (`ToolDef`) and fills in safe defaults, returning a complete `Tool` object.

### The buildTool Factory

```typescript
// src/Tool.ts:783
export function buildTool<D extends AnyToolDef>(def: D): BuiltTool<D> {
  return {
    ...TOOL_DEFAULTS,
    userFacingName: () => def.name,
    ...def,
  } as BuiltTool<D>
}
```

The defaults that `buildTool` supplies (src/Tool.ts:757-769) are fail-closed where it matters:

| Method | Default | Rationale |
|--------|---------|-----------|
| `isEnabled` | `() => true` | Tools are enabled unless they opt out |
| `isConcurrencySafe` | `() => false` | Assume NOT safe for parallel execution |
| `isReadOnly` | `() => false` | Assume the tool writes data |
| `isDestructive` | `() => false` | Not irreversible by default |
| `checkPermissions` | `allow` | Defer to the general permission system |
| `toAutoClassifierInput` | `() => ''` | Skip auto-mode classifier unless overridden |
| `userFacingName` | `() => def.name` | Falls back to the tool's registered name |

### Tool Type Shape

The full `Tool` type (src/Tool.ts:362-695) is a large interface with these key method groups:

1. **Identity**: `name`, `aliases`, `searchHint`, `shouldDefer`, `alwaysLoad`, `isMcp`, `isLsp`
2. **Schema**: `inputSchema` (Zod), `outputSchema` (Zod), `inputJSONSchema` (raw JSON Schema for MCP)
3. **Lifecycle**: `validateInput`, `checkPermissions`, `call`, `isEnabled`
4. **Behavior flags**: `isConcurrencySafe`, `isReadOnly`, `isDestructive`, `interruptBehavior`
5. **Rendering**: `renderToolUseMessage`, `renderToolResultMessage`, `renderToolUseProgressMessage`, `renderToolUseErrorMessage`, `renderToolUseRejectedMessage`, `renderGroupedToolUse`
6. **Serialization**: `mapToolResultToToolResultBlockParam` (converts output to API-compatible format)
7. **Classifier**: `toAutoClassifierInput`, `preparePermissionMatcher`
8. **UI helpers**: `userFacingName`, `getToolUseSummary`, `getActivityDescription`, `isSearchOrReadCommand`

## Tool Execution Pipeline

When the model emits a `tool_use` block, the following pipeline executes:

### 1. Input Parsing and Backfill

The raw JSON input from the model is parsed against the tool's `inputSchema` (Zod). If the tool defines `backfillObservableInput`, it is called on a copy of the input to add derived fields. For example, FileReadTool expands `~` and relative paths to absolute paths (src/tools/FileReadTool/FileReadTool.ts:391-393):

```typescript
backfillObservableInput(input) {
  if (typeof input.file_path === 'string') {
    input.file_path = expandPath(input.file_path)
  }
}
```

### 2. Input Validation

`validateInput()` runs BEFORE permission checks. This is pure validation -- no user-facing UI. It returns either `{ result: true }` or `{ result: false, message, errorCode }`. Common validation patterns:

- **Path existence**: FileReadTool checks blocked device paths (`/dev/zero`, `/dev/stdin`), binary extensions, deny rules
- **Staleness**: FileEditTool and FileWriteTool verify the file hasn't been modified since last read (mtime comparison)
- **Read-before-write**: FileEditTool/FileWriteTool/NotebookEditTool require the file to be in `readFileState` before allowing edits
- **UNC path security**: All file tools skip filesystem operations for `\\` and `//` paths to prevent NTLM credential leaks

### 3. Permission Checks

`checkPermissions()` is called after validation passes. Returns a `PermissionResult` with behaviors:

- `allow` -- proceed immediately
- `deny` -- block with message
- `ask` -- prompt the user for permission
- `passthrough` -- defer to the general permission system

Tools also implement `preparePermissionMatcher()` which returns a closure for matching hook `if` conditions. For BashTool, this parses compound commands so that `ls && git push` correctly triggers a `Bash(git *)` security hook (src/tools/BashTool/BashTool.tsx:446-467).

### 4. Tool Execution

`call()` is the main execution method. It receives:
- Parsed input (Zod-validated)
- `ToolUseContext` (the rich execution environment)
- `canUseTool` function (for tools that spawn sub-tool calls)
- `parentMessage` (the assistant message that triggered this tool use)
- `onProgress` callback (optional, for streaming progress updates)

### 5. Result Formatting

After `call()` returns a `ToolResult<T>`, the result goes through:

1. `mapToolResultToToolResultBlockParam()` -- converts the typed output into the `ToolResultBlockParam` format the Anthropic API expects
2. The UI renders via `renderToolResultMessage()` for the terminal display
3. Large results exceeding `maxResultSizeChars` get persisted to disk, with a preview sent to the model

## The ToolUseContext

Every tool call receives a `ToolUseContext` (src/Tool.ts:158-300) which provides:

- **State**: `getAppState()`, `setAppState()` -- access to global application state
- **File cache**: `readFileState` -- a `FileStateCache` tracking file contents and mtimes for staleness detection
- **Abort**: `abortController` -- for cancellation
- **Limits**: `fileReadingLimits`, `globLimits` -- configurable caps
- **Messages**: `messages` -- the conversation history
- **Options**: `options.tools`, `options.mcpClients`, `options.thinkingConfig`, `options.mainLoopModel`
- **UI**: `setToolJSX`, `addNotification`, `sendOSNotification`
- **Agent context**: `agentId`, `agentType` -- set for subagents
- **Tracking**: `toolDecisions`, `queryTracking`, `contentReplacementState`

## Concurrency and Parallel Execution

Tools declare concurrency safety via `isConcurrencySafe(input)`. When multiple tool calls arrive in the same assistant message:

- **Concurrency-safe tools** (Read, Grep, Glob, WebFetch, WebSearch) can execute in parallel
- **Non-safe tools** (Bash, Edit, Write) execute sequentially
- BashTool is special: it's concurrency-safe only when `isReadOnly()` returns true (src/tools/BashTool/BashTool.tsx:434-436)

The `isReadOnly()` check for BashTool delegates to `checkReadOnlyConstraints()` which parses the command to determine if it modifies state.

## Tool Result Persistence

When a tool result exceeds `maxResultSizeChars`, it gets written to disk in a tool-results directory, and the model receives a preview with a file path. Key thresholds:

| Tool | maxResultSizeChars |
|------|-------------------|
| BashTool | 30,000 |
| GrepTool | 20,000 |
| FileEditTool | 100,000 |
| FileWriteTool | 100,000 |
| FileReadTool | Infinity (never persisted -- circular read loop) |
| MCPTool | 100,000 |
| AgentTool | 100,000 |

The special case of `Infinity` for FileReadTool (src/tools/FileReadTool/FileReadTool.ts:342) prevents a circular dependency where persisting the Read output creates a file that the model would need to Read again.

## Deferred Tools (ToolSearch)

Tools can declare `shouldDefer: true` to be loaded lazily. When deferred, the tool schema is sent with `defer_loading: true` -- the model must call `ToolSearch` first to discover the tool before using it. Deferred tools include: WebFetchTool, WebSearchTool, LSPTool, NotebookEditTool, EnterPlanModeTool, ExitPlanModeV2Tool, EnterWorktreeTool, SkillTool, all Task tools, TeamCreateTool, CronCreateTool.

Tools can override this with `alwaysLoad: true` to force their schema into every prompt regardless of ToolSearch.

## The Shared Infrastructure

### Git Operation Tracking (src/tools/shared/gitOperationTracking.ts)

Monitors BashTool/PowerShellTool command output for git operations (commits, pushes, PR creation) and fires analytics events + increments OTLP counters. Detects `git commit`, `git push`, `gh pr create/edit/merge/comment/close/ready`, `glab mr create`, and even `curl` POST to PR endpoints. This feeds the collapsed tool-use summary ("committed a1b2c3, created PR #42").

### Teammate Spawning (src/tools/shared/spawnMultiAgent.ts)

The shared spawn module handles creating new Claude Code instances as teammates. Three backends:

1. **Split-pane** (default): Creates teammates in tmux/iTerm2 split panes
2. **Separate window**: Creates each teammate in its own tmux window
3. **In-process**: Runs teammates in the same Node.js process using AsyncLocalStorage

The spawn module resolves teammate models (handling the `inherit` alias), generates unique names, assigns colors, registers tasks, and sends initial prompts via mailbox.

## Lazy Schema Pattern

All tool schemas use `lazySchema()` -- a wrapper that defers Zod schema construction until first access. This is critical for startup performance: schema parsing is expensive, and many tools may never be used in a given session. The pattern:

```typescript
const inputSchema = lazySchema(() =>
  z.strictObject({
    command: z.string().describe('The command to execute'),
    timeout: z.number().optional(),
  })
)
```

## Permission Modes

The `ToolPermissionContext` (src/Tool.ts:123-138) supports these modes:

- `default` -- standard interactive approval
- `bypassPermissions` -- skip all permission checks (`--dangerously-skip-permissions`)
- `acceptEdits` -- auto-approve file edits
- `auto` -- AI classifier auto-approves based on transcript analysis
- `plan` -- read-only exploration mode (blocks writes, allows reads)

Each mode affects which tools require user confirmation and which auto-approve.
