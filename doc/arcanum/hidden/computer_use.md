---
description: "computer use Chicago — screenshot capture, mouse click keyboard input, MCP server integration, feature-gated, desktop automation, UI interaction"
---

# Computer Use ("Chicago") -- Arcanum Wiki

## What Is This?

Computer Use, internally codenamed **"Chicago"**, is Claude Code's ability to directly control a macOS computer -- clicking, typing, scrolling, dragging, taking screenshots, and managing applications. It wraps two native modules: a Rust/enigo module (`@ant/computer-use-input`) for mouse and keyboard input, and a Swift module (`@ant/computer-use-swift`) for screenshots via SCContentFilter and application management via NSWorkspace.

The feature is exposed as an MCP server (`mcp__computer-use__*` tools) and is currently restricted to **macOS only** and **Max/Pro subscription tiers** (or internal Anthropic users). The MCP naming is deliberate: the API backend detects `mcp__computer-use__*` tool names and injects a computer-use availability hint into the system prompt.

## How It Works

### Architecture

The system has three layers:

1. **Gates** (`gates.ts`) -- GrowthBook-controlled feature flags and subscription checks
2. **Setup** (`setup.ts`) -- Builds MCP config and tool registration
3. **Executor** (`executor.ts`) -- The actual `ComputerExecutor` implementation with mouse, keyboard, screenshot, and app management

### Feature Gating (gates.ts)

Configuration comes from GrowthBook dynamic config `tengu_malort_pedway`:

```typescript
const DEFAULTS: ChicagoConfig = {
  enabled: false,
  pixelValidation: false,
  clipboardPasteMultiline: true,
  mouseAnimation: true,
  hideBeforeAction: true,
  autoTargetDisplay: true,
  clipboardGuard: true,
  coordinateMode: 'pixels',
}
```

Subscription check requires Max or Pro tier, with an Anthropic employee bypass:

```typescript
function hasRequiredSubscription(): boolean {
  if (process.env.USER_TYPE === 'ant') return true
  const tier = getSubscriptionType()
  return tier === 'max' || tier === 'pro'
}
```

A special exclusion prevents ants running from the monorepo from accidentally triggering computer use (unless `ALLOW_ANT_COMPUTER_USE_MCP=1`).

The coordinate mode is **frozen at first read** -- a mid-session GrowthBook config change cannot cause the model to think "pixels" while the executor transforms as "normalized".

### Executor (executor.ts) -- 659 lines

The executor is the heart of the system. Key capabilities:

**Mouse operations**: Move, click (left/right/middle, single/double/triple), drag, scroll. Mouse moves use an instant jump + 50ms settle for HID round-trip time. Drag operations use animated movement (ease-out-cubic at 60fps, 2000px/sec, capped at 0.5s).

**Keyboard**: xdotool-style key sequences (`ctrl+shift+a`), hold keys for duration, typing text (direct or via clipboard paste). The Escape key gets special treatment -- a CGEventTap abort callback is notified before model-synthesized Escape presses so it does not fire the safety abort.

**Clipboard paste** (`typeViaClipboard`): A careful 6-step sequence:
1. Save user's clipboard
2. Write our text
3. Read-back verify (clipboard writes can silently fail)
4. Cmd+V via keys
5. Sleep 100ms for paste-effect vs clipboard-restore race
6. Restore original clipboard in `finally` block

**Screenshots**: Pre-sized to `targetImageSize` output so the API transcoder's early-return fires -- no server-side resize, coordinate scaling stays coherent. The terminal emulator is stripped from screenshots via `withoutTerminal()` so it never "photobombs" a screenshot.

**Application management**: Hide/unhide apps, list installed/running apps, get frontmost app, detect app under cursor position, open apps by bundle ID.

### Terminal as Surrogate Host

Since Claude Code runs in a terminal (no window), the executor uses the terminal's bundle ID as a surrogate host:

```typescript
const surrogateHost = terminalBundleId ?? CLI_HOST_BUNDLE_ID
```

Terminal detection covers iTerm, Apple Terminal, Ghostty, Kitty, Warp, VSCode via both `__CFBundleIdentifier` and a fallback table. The sentinel `CLI_HOST_BUNDLE_ID` (`com.anthropic.claude-code.cli-no-window`) never matches a real frontmost application.

### CFRunLoop Pumping

Unlike Cowork (which runs in Electron and drains CFRunLoop continuously), Claude Code needs explicit pumping via `drainRunLoop()`. Without it, window-manager events pile up during Swift's sleep calls and flush all at once, causing visible window flashing.

## Feature Gating

| Gate | Type | Control |
|------|------|---------|
| `tengu_malort_pedway.enabled` | GrowthBook dynamic config | Master switch |
| Subscription tier | Runtime auth check | Max or Pro required |
| `USER_TYPE === 'ant'` | Env var | Anthropic employee bypass |
| `ALLOW_ANT_COMPUTER_USE_MCP` | Env var | Override monorepo exclusion |
| `process.platform === 'darwin'` | Runtime | macOS only |

## User-Facing Behavior

When enabled, Claude can:
- Take screenshots of your desktop (excluding the terminal)
- Click, type, scroll, and drag on screen elements
- Open, hide, and manage applications
- Read and write the clipboard
- Perform multi-step automation sequences

Before each action, `prepareForAction` can hide irrelevant windows and activate the target application. The Escape key acts as an abort mechanism via a CGEventTap.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/utils/computerUse/executor.ts` | Core ComputerExecutor implementation (659 lines) |
| `src/utils/computerUse/gates.ts` | GrowthBook config and subscription gating |
| `src/utils/computerUse/setup.ts` | MCP server config and tool registration |
| `src/utils/computerUse/common.ts` | Constants, terminal bundle ID detection |
| `src/utils/computerUse/drainRunLoop.ts` | CFRunLoop pump for Swift interop |
| `src/utils/computerUse/escHotkey.ts` | Escape key abort mechanism |
| `src/utils/computerUse/inputLoader.ts` | Lazy loader for Rust input module |
| `src/utils/computerUse/swiftLoader.ts` | Lazy loader for Swift screenshot module |
| `src/utils/computerUse/hostAdapter.ts` | Host adapter interface |
| `src/utils/computerUse/computerUseLock.ts` | Concurrency lock |
| `src/utils/computerUse/appNames.ts` | Application name resolution |
| `src/utils/computerUse/cleanup.ts` | Cleanup on session end |
| `src/utils/computerUse/mcpServer.ts` | In-process MCP server |
| `src/utils/computerUse/wrapper.tsx` | React wrapper component |
| `src/utils/computerUse/toolRendering.tsx` | Tool result rendering |

## Configuration

- `tengu_malort_pedway` GrowthBook config controls all sub-features
- Coordinate mode frozen at first read to prevent mid-session drift
- No user-facing settings -- entirely server-controlled

## Interesting Findings

1. **The codename "Chicago"** and the GrowthBook key `tengu_malort_pedway` (Malort is a notoriously bitter Chicago liqueur, Pedway is Chicago's underground walkway system) continue the theme. "Chicago" is also the name used in the Anthropic apps repo.

2. **Screenshot filtering is "native"** -- the Swift module uses SCContentFilter to exclude specific windows from screenshots, rather than capturing everything and masking. This is both more efficient and more secure.

3. **The 8ms sleep between key repeat iterations** matches the 125Hz USB polling cadence -- a deliberate alignment with HID timing.

4. **The modifier key safety** is thorough: `withModifiers` tracks which keys were actually pressed, so a mid-press throw only releases what was pressed. `releasePressed` drains via `pop()` rather than snapshotting length to handle race conditions with orphaned press lambdas.

5. **The reference implementation** is Cowork's executor at `apps/desktop/src/main/nest-only/computer-use/executor.ts`. The CLI version has documented deltas: no click-through (no window), terminal as surrogate host, clipboard via pbcopy/pbpaste instead of Electron's clipboard module.
