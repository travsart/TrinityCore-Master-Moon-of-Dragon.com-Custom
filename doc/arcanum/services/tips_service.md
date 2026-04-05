---
description: "tips service — contextual tips display, usage hints, feature discovery prompts, tip rotation and dismissal"
---

# Tips Service -- Arcanum Wiki

## What Is This?

The Tips service displays contextual productivity tips in the spinner (loading indicator) while Claude Code is processing. Tips are selected based on the user's environment, recent tool usage, installed features, and how recently each tip was shown. The system is designed to educate users about features they might not know about.

## How It Works

### Tip Registry (`tipRegistry.ts`)

The registry defines all available tips with relevance functions that evaluate:
- Which tools were used (bash commands, file operations)
- Which IDEs are running (VS Code, Cursor, Windsurf)
- Platform (macOS, Linux, Windows)
- User type (consumer, API customer, Ant)
- Feature state (file history enabled, terminal setup offered, effort support)
- Model capabilities
- Session count and configuration

Each tip has:
- `id`: Unique identifier
- `message`: Display text (supports chalk formatting)
- `cooldownSessions`: Minimum sessions between showings
- `isRelevant(context)`: Async function that determines if the tip applies now

### Tip Scheduler (`tipScheduler.ts`)

`getTipToShowOnSpinner()` is called when the spinner starts:
1. Checks if tips are enabled in settings (default: true)
2. Gets all relevant tips via `getRelevantTips(context)`
3. Selects the tip with the longest time since last shown (`selectTipWithLongestTimeSinceShown`)

This ensures even distribution -- no single tip dominates.

### Tip History (`tipHistory.ts`)

Tracks which tips have been shown and when, persisted across sessions. `getSessionsSinceLastShown(tipId)` returns how many sessions have elapsed since a tip was displayed.

### Context-Aware Examples

Tips check environmental signals. For example:
- A terminal setup tip only shows if `shouldOfferTerminalSetup()` returns true
- IDE-specific tips check `detectRunningIDEsCached()` and `isSupportedVSCodeTerminal()`
- Plugin marketplace tips check if the official marketplace is installed and whether specific plugins are relevant based on file types in the project
- Overage credit upsell tips check subscription state
- Effort control tips check model support

## Key Source Files

| File | Purpose |
|------|---------|
| `tipRegistry.ts` | All tip definitions with relevance functions |
| `tipScheduler.ts` | Selection algorithm and display lifecycle |
| `tipHistory.ts` | Persistence of tip display history |

## Configuration

- Settings: `spinnerTipsEnabled` (default true)
- Per-tip: `cooldownSessions` prevents re-showing too soon
- Analytics: `tengu_tip_shown` event logged per display

## Interesting Findings

1. **Tips are async-evaluated** -- relevance functions can perform filesystem checks, config reads, and IDE detection. This is fine because tips only need to resolve before the spinner renders.

2. **The LRU selection algorithm** (longest time since shown) ensures users see a variety of tips rather than the same one repeatedly.

3. **Plugin marketplace tips** are particularly sophisticated -- they check if the official marketplace is installed, whether the specific plugin is already installed, and whether the project contains relevant file types (e.g., suggesting a Docker plugin when Dockerfiles are present).
