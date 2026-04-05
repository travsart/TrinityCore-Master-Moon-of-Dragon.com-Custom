---
description: "fun hidden commands — /stickers stickermule, /good-claude disabled stub, /btw, /thinkback year-in-review, /thinkback-play, /insights analytics"
---

# Fun & Hidden Commands -- Arcanum Wiki

## Overview

These commands range from whimsical features to useful side-channel communication tools. Some are Easter eggs, some are experimental features gated behind flags, and some are genuinely useful workflow tools that just happen to have playful names.

## Commands

### /stickers
- **Arguments**: None
- **What it does**: Opens the Claude Code sticker ordering page in your default browser. Links to `https://www.stickermule.com/claudecode` where users can buy physical Claude Code stickers.
- **Feature gating**: None -- always available. Not available in non-interactive mode.
- **Key code**:
```typescript
export async function call(): Promise<LocalCommandResult> {
  const url = 'https://www.stickermule.com/claudecode'
  const success = await openBrowser(url)
  if (success) {
    return { type: 'text', value: 'Opening sticker page in browser...' }
  } else {
    return { type: 'text', value: `Failed to open browser. Visit: ${url}` }
  }
}
```
- **Notes**: One of the simplest commands in the codebase -- just opens a URL.

---

### /good-claude
- **Arguments**: Unknown (stubbed)
- **What it does**: STUBBED OUT. The name suggests this was (or will be) a positive reinforcement / acknowledgment command -- perhaps letting users praise Claude for good work. Given the tamagotchi-like "buddy" system that exists elsewhere in the codebase, this may have been part of a gamification feature.
- **Feature gating**: `isEnabled: () => false`, `isHidden: true`

---

### /btw
- **Arguments**: `<question>`
- **What it does**: Asks a quick side question without interrupting the main conversation. This creates a "sidechain" -- a separate conversation branch that:
  1. Takes the current conversation context
  2. Sends the question to the model in isolation
  3. Shows the response in a scrollable overlay panel
  4. Does NOT affect the main conversation history

  The UI shows a spinner while loading, then renders the response in markdown. Users can scroll with arrow keys and dismiss with ESC, Enter, or Space.
- **Feature gating**: None -- always available.
- **Execution**: `immediate: true` -- does not wait for main loop.
- **Key code**:
```typescript
const btw = {
  type: 'local-jsx',
  name: 'btw',
  description: 'Ask a quick side question without interrupting the main conversation',
  immediate: true,
  argumentHint: '<question>',
}
```
- **Notes**: Uses `runSideQuestion()` which creates a separate API call with `CacheSafeParams` to maintain prompt cache efficiency. The response overlay has a fixed chrome of 5-6 rows for borders/instructions.

---

### /think-back
- **Arguments**: None
- **What it does**: Shows "Your 2025 Claude Code Year in Review" -- a personalized retrospective of the user's Claude Code usage over the past year. Think Spotify Wrapped but for coding with Claude.
- **Feature gating**: Gated behind `tengu_thinkback` Statsig feature gate. Only shown when the gate is enabled.
- **Key code**:
```typescript
const thinkback = {
  type: 'local-jsx',
  name: 'think-back',
  description: 'Your 2025 Claude Code Year in Review',
  isEnabled: () =>
    checkStatsigFeatureGate_CACHED_MAY_BE_STALE('tengu_thinkback'),
}
```

---

### /thinkback-play
- **Arguments**: None
- **What it does**: Plays the thinkback animation. This is a hidden companion command to `/think-back` -- after the thinkback skill generates the year-in-review content, this command is called to play the visual animation.
- **Feature gating**: Same gate as `/think-back` (`tengu_thinkback`). Permanently hidden (`isHidden: true`).
- **Key code**:
```typescript
// Hidden command that just plays the animation
// Called by the thinkback skill after generation is complete
const thinkbackPlay = {
  type: 'local',
  name: 'thinkback-play',
  description: 'Play the thinkback animation',
  isEnabled: () =>
    checkStatsigFeatureGate_CACHED_MAY_BE_STALE('tengu_thinkback'),
  isHidden: true,
  supportsNonInteractive: false,
}
```
- **Notes**: This is purely internal -- users invoke `/think-back` and the skill orchestrates calling `/thinkback-play` when ready.

---

### /insights
- **Arguments**: Not exposed as a standard slash command
- **What it does**: A standalone `.ts` file (not a subdirectory command) that provides deep analysis of session history. It:
  1. Loads session files from the projects directory
  2. Extracts text content from messages
  3. Uses the Opus model for "facet extraction and summarization"
  4. Performs diff analysis between sessions
  5. Generates insights reports

  This appears to be the data-gathering backend for features like `/think-back`.
- **Feature gating**: Not directly exposed as a user-facing command.
- **Key code**:
```typescript
// Model for facet extraction and summarization (Opus - best quality)
import { getDefaultOpusModel } from '../../utils/model/model.js'
```
- **Notes**: Uses `queryWithModel` to send session data to the model for analysis. Works with JSONL session files and handles content replacement entries.

## Hidden/Undocumented Commands

- **/good-claude** -- Stubbed out, permanently disabled. Easter egg that never shipped.
- **/thinkback-play** -- Functional but hidden. Internal animation player for the year-in-review feature.
- **/insights** -- Not a standard slash command; standalone utility file.
- **/btw** -- Visible and functional, but many users don't know about it. It's genuinely one of the most useful hidden-in-plain-sight features.
