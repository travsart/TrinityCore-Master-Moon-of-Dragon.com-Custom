---
description: "misc tools — BriefTool, ConfigTool, PowerShellTool, REPLTool, RemoteTriggerTool, SyntheticOutputTool, TodoWriteTool, ToolSearchTool, deferred tool lazy loading"
---

# Miscellaneous Tools -- Arcanum Wiki

Covers smaller and utility tools: BriefTool, ConfigTool, PowerShellTool, REPLTool, RemoteTriggerTool, SyntheticOutputTool, TodoWriteTool, and ToolSearchTool.

---

## ToolSearchTool

### Purpose
Enables lazy loading of deferred tools. When many tools are available (especially with MCP servers), ToolSearchTool provides a keyword search or direct selection mechanism for tools that are deferred (`shouldDefer: true`). Without this, the model would need every tool's full schema in every prompt.

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `query` | string | Yes | `select:<tool_name>` for direct selection, or keywords to search |
| `max_results` | number | No | Maximum results (default 5) |

### Key Details
- Searches deferred tool names, search hints, and full prompts/descriptions
- Uses memoized description loading with cache invalidation when tool set changes
- Direct selection via `select:ToolName` bypasses keyword matching
- Returns tool names that the model can then call
- Enabled when `isToolSearchEnabledOptimistic()` returns true (based on total tool count threshold)
- `alwaysLoad: true` -- never deferred itself

---

## BriefTool

### Purpose
Sends a formatted message to the user with optional file attachments. Used in assistant/Kairos mode for proactive notifications and status updates. Supports images, screenshots, diffs, and logs as attachments.

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `message` | string | Yes | Markdown-formatted message for the user |
| `attachments` | string[] | No | File paths to attach (images, logs, etc.) |
| `status` | enum | Yes | `normal` for replies, `proactive` for unsolicited updates |

### Key Details
- Resolves attachment paths and validates they exist
- Captures ISO timestamp at execution time (`sentAt`)
- Aliases: `Brief` (primary), legacy name exists
- Gated by Kairos/Brief feature flags plus GrowthBook runtime gate
- `shouldDefer: true` when not in brief mode
- Sends OS notifications for proactive messages

---

## TodoWriteTool

### Purpose
Updates the session's todo/checklist. This is the legacy todo system (v1) -- it is only enabled when `isTodoV2Enabled()` returns FALSE (mutually exclusive with the Task tools).

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `todos` | TodoList | Yes | The complete updated todo list |

### Key Details
- Replaces the entire todo list atomically (not incremental updates)
- When all items are `completed`, clears the list entirely
- **Verification nudge**: When 3+ tasks are closed at once with no "verification" item, appends a reminder to spawn a verification agent before writing the final summary (behind `tengu_hive_evidence` flag)
- No permission checks required
- `shouldDefer: true`
- `renderToolUseMessage` returns null (no UI for todo writes -- the todo panel updates instead)

---

## SyntheticOutputTool

### Purpose
Returns structured JSON output in SDK/CLI (non-interactive) mode. This is Claude Code's mechanism for structured output -- the model calls this tool to return data matching a user-provided JSON schema.

### Parameters
Dynamic -- accepts any object matching the user-provided JSON schema.

### Key Details
- Named `StructuredOutput` internally
- Only enabled in non-interactive sessions (`isNonInteractiveSession`)
- Validates output against the provided JSON schema using Ajv
- Schema compilation is cached via WeakMap for performance (80-call workflows: ~110ms to ~4ms)
- `createSyntheticOutputTool()` factory creates a configured version with schema validation
- Always allowed (no permission checks)
- Concurrency safe, read only

---

## ConfigTool

### Purpose
Allows the model to read and update Claude Code configuration settings. Supports viewing current settings and modifying supported configuration values.

### Key Details
- Lists supported settings from `supportedSettings.ts`
- Can read and write configuration values
- `shouldDefer: true`
- Changes affect the current session and/or persistent settings

---

## PowerShellTool

### Purpose
Executes PowerShell commands on Windows. Similar to BashTool but for PowerShell-specific commands. Shares git operation tracking infrastructure.

### Key Details
- Only enabled on Windows
- Shares the same `gitOperationTracking.ts` module as BashTool
- Similar progress streaming, background execution, and sandboxing capabilities
- Parameters mirror BashTool: `command`, `timeout`, `description`, `run_in_background`

---

## REPLTool

### Purpose
Runs code in a REPL (Read-Eval-Print Loop) environment. Serves as a transparent wrapper that delegates to BashTool/PowerShellTool, making it appear as though each inner tool call is native.

### Key Details
- `isTransparentWrapper()` returns true -- rendering delegates to progress handler
- Emits native-looking blocks for each inner tool call
- The wrapper itself shows nothing in the UI
- Supports multiple language REPLs

---

## RemoteTriggerTool

### Purpose
Triggers actions on a remote Claude Code instance. Used in multi-environment setups where one Claude Code session needs to communicate with another.

### Key Details
- `shouldDefer: true`
- Takes an action/trigger parameter
- Used in remote/distributed agent scenarios

---

## Interesting Findings Across Misc Tools

1. **ToolSearchTool is never deferred itself** (`alwaysLoad: true`). It must always be available so the model can discover other deferred tools. This creates a bootstrap requirement -- ToolSearchTool is the gatekeeper.

2. **TodoWriteTool and Task tools are mutually exclusive**: TodoWrite is enabled when `!isTodoV2Enabled()`, while all Task tools are enabled when `isTodoV2Enabled()`. This is a migration path from the old atomic-replace todo system to the new incremental task system.

3. **SyntheticOutputTool uses Ajv, not Zod** for schema validation. This is because the user provides a JSON Schema (from SDK's `schema` parameter), and Ajv is the standard JSON Schema validator. The WeakMap cache on schema object identity is a clever optimization for workflow scripts that call `agent()` 30-80 times with the same schema reference.

4. **BriefTool's attachments field is deliberately optional in the output schema** -- resumed sessions replay pre-attachment outputs verbatim, and a required field would crash the UI renderer on resume. This is noted explicitly in the source.

5. **The verification nudge in TodoWriteTool** is a structural enforcement mechanism: when the model closes 3+ tasks without any verification step, it's told it "cannot self-assign PARTIAL by listing caveats" and must spawn a verification agent. This is Anthropic's approach to preventing premature task completion claims.
