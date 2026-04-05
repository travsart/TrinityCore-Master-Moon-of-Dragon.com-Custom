---
description: "context debug commands — /context /ctx_viz /compact /cost /stats /usage /extra-usage /rate-limit-options, token counts, context visualization"
---

# Context & Debug Commands -- Arcanum Wiki

## Overview

These commands help users understand and manage their context window -- the limited token budget that determines how much conversation history the model can see. They also include cost tracking and usage statistics.

## Commands

### /context
- **Arguments**: None
- **What it does**: Visualizes the current context window usage as a colored grid. This is one of the most informative diagnostic commands. It shows:
  - Total token usage vs. available context window
  - Per-message token breakdown
  - System prompt size
  - Tool definitions size
  - Collapsed context spans (when CONTEXT_COLLAPSE feature is enabled)

  The implementation applies the same transforms that `query.ts` does before API calls, so the visualization shows what the model **actually sees**, not the raw REPL history. Without this, token counts would overcount by however much was collapsed.
- **Feature gating**: Disabled in non-interactive sessions. Has a separate non-interactive variant (`contextNonInteractive`) that outputs plain text.
- **Key code**:
```typescript
// Apply the same context transforms query.ts does before the API call
function toApiView(messages: Message[]): Message[] {
  let view = getMessagesAfterCompactBoundary(messages)
  if (feature('CONTEXT_COLLAPSE')) {
    const { projectView } = require('../../services/contextCollapse/operations.js')
    view = projectView(view)
  }
  return view
}
```
- **Notes**: Uses `microcompactMessages` to get accurate representation of messages sent to API. Renders a `ContextVisualization` React component to ANSI string output.

---

### /ctx_viz
- **Arguments**: Unknown (stubbed)
- **What it does**: STUBBED OUT. Permanently disabled. Was likely an alternative or earlier version of the context visualization.
- **Feature gating**: `isEnabled: () => false`, `isHidden: true`

---

### /compact
- **Arguments**: `<optional custom summarization instructions>`
- **What it does**: Clears conversation history but preserves a compressed summary in context. This is the primary way to free up context when approaching the limit. The compaction process:
  1. First tries "session memory compaction" (if no custom instructions provided) -- a lighter-weight approach
  2. Falls back to full compaction which sends conversation to the model for summarization
  3. Runs pre-compact hooks (user-configurable)
  4. Applies post-compact cleanup
  5. Notifies prompt cache break detection

  Custom instructions let users guide what the summary should focus on, e.g., `/compact focus on the database migration work`.
- **Feature gating**: Can be disabled via `DISABLE_COMPACT` environment variable. Supports non-interactive mode.
- **Key code**:
```typescript
const compact = {
  type: 'local',
  name: 'compact',
  description: 'Clear conversation history but keep a summary in context. ' +
    'Optional: /compact [instructions for summarization]',
  isEnabled: () => !isEnvTruthy(process.env.DISABLE_COMPACT),
  supportsNonInteractive: true,
  argumentHint: '<optional custom summarization instructions>',
}
```
- **Notes**: The implementation imports from several compaction services: `compactConversation`, `microcompactMessages`, `trySessionMemoryCompaction`, and `reactiveCompact` (feature-gated). Error messages include `ERROR_MESSAGE_INCOMPLETE_RESPONSE`, `ERROR_MESSAGE_NOT_ENOUGH_MESSAGES`, and `ERROR_MESSAGE_USER_ABORT`.

---

### /cost
- **Arguments**: None
- **What it does**: Shows the total cost and duration of the current session. Displays API costs, token counts, and timing information.
- **Feature gating**: Hidden for Claude AI subscribers (they don't pay per-token), EXCEPT for Anthropic employees (`USER_TYPE === 'ant'`) who always see it for debugging purposes.
- **Key code**:
```typescript
const cost = {
  type: 'local',
  name: 'cost',
  description: 'Show the total cost and duration of the current session',
  get isHidden() {
    if (process.env.USER_TYPE === 'ant') return false
    return isClaudeAISubscriber()
  },
  supportsNonInteractive: true,
}
```

---

### /stats
- **Arguments**: None
- **What it does**: Shows Claude Code usage statistics and activity. Displays aggregate data about how you've been using Claude Code, including conversation counts, tool usage patterns, and activity over time.
- **Feature gating**: None -- always available.
- **Key code**:
```typescript
const stats = {
  type: 'local-jsx',
  name: 'stats',
  description: 'Show your Claude Code usage statistics and activity',
}
```

---

### /usage
- **Arguments**: None
- **What it does**: Shows plan usage limits -- how much of your subscription quota you've consumed. Displays remaining API calls, token budgets, and time until reset.
- **Feature gating**: Only available for `claude-ai` availability context (Claude AI subscribers).
- **Key code**:
```typescript
export default {
  type: 'local-jsx',
  name: 'usage',
  description: 'Show plan usage limits',
  availability: ['claude-ai'],
}
```

---

### /extra-usage
- **Arguments**: None
- **What it does**: Configures "extra usage" -- the ability to continue working when rate limits are hit by paying for additional capacity. This is the overage provisioning system.
- **Feature gating**: Requires `isOverageProvisioningAllowed()` to return true. Can be disabled via `DISABLE_EXTRA_USAGE_COMMAND` env var. Disabled in non-interactive sessions. Has a separate non-interactive variant.
- **Key code**:
```typescript
export const extraUsage = {
  type: 'local-jsx',
  name: 'extra-usage',
  description: 'Configure extra usage to keep working when limits are hit',
  isEnabled: () => isExtraUsageAllowed() && !getIsNonInteractiveSession(),
}
```

---

### /rate-limit-options
- **Arguments**: None
- **What it does**: Shows options when rate limit is reached. This is an internal-use command that surfaces upgrade paths, extra usage options, and wait time estimates.
- **Feature gating**: Only enabled for Claude AI subscribers. Permanently hidden from help (`isHidden: true`) -- used internally by the rate limit handling flow.
- **Key code**:
```typescript
const rateLimitOptions = {
  type: 'local-jsx',
  name: 'rate-limit-options',
  description: 'Show options when rate limit is reached',
  isEnabled: () => isClaudeAISubscriber(),
  isHidden: true,
}
```
- **Notes**: This is never shown in `/help` -- it's triggered programmatically when users hit rate limits.

## Hidden/Undocumented Commands

- **/ctx_viz** -- Stubbed out, permanently disabled. Dead code.
- **/rate-limit-options** -- Functional but permanently hidden. Only invoked programmatically by the rate limit system.
- **/cost** -- Hidden for Claude AI subscribers but visible for API key users and Anthropic employees.
