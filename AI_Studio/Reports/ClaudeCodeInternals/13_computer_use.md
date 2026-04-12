# 13. Computer Use ("Chicago")

> Source: `src/utils/computerUse/` (15 files, ~2,161 lines) — v2.1.88 source
> Feature flags: `feature('CHICAGO_MCP')` (compile-time) + `tengu_malort_pedway` (GrowthBook runtime)
> Platform: macOS ONLY (darwin guard, throws on other platforms)
> Status: GATED -- Max/Pro subscribers only, disabled by default
>
> **Verified unchanged vs v2.1.97 cli.js** (grep spot-check, 2026-04-08): all 11 tool names (`request_access`, `list_granted_applications`, `computer_batch`, `left_click_drag`, `double_click`, `triple_click`, `hold_key`, `open_application`, `read_clipboard`, etc.), both native NAPI paths (`COMPUTER_USE_INPUT_NODE_PATH`, `COMPUTER_USE_SWIFT_NODE_PATH`), the libuv pump (`drainRunLoop` / `_drainMainRunLoop`), the GrowthBook flag (`tengu_malort_pedway`), the macOS notification string ("Claude is using your computer"), and the MCP server name (`computer-use`) are all present in cli.js@2.1.97.

## Executive Summary

Computer Use (codename "Chicago") gives Claude the ability to control the user's computer -- taking screenshots, clicking, typing, scrolling, and managing applications. It is implemented as an in-process MCP server backed by two native modules: Rust/enigo for input simulation and Swift for screenshots via SCContentFilter. The system has three layers of feature gating (compile-time, runtime config, subscription tier) and extensive security measures including app allowlisting, ESC abort hotkey, session locking, and prompt-injection hardening.

**Key finding**: Tools are named `mcp__computer-use__*` because the API backend specifically detects this prefix and injects a CU availability hint into the system prompt. Custom names would not trigger it.

## Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                        Model (Claude)                            │
│            Sees mcp__computer-use__* tools in prompt             │
├──────────────────────────────────────────────────────────────────┤
│                    MCP Tool Dispatch Layer                        │
│  client.ts intercepts → wrapper.tsx .call() override             │
│  Lock acquire → Permission dialog → Session binding              │
├──────────────┬───────────────────────┬───────────────────────────┤
│  setup.ts    │    hostAdapter.ts     │   toolRendering.tsx       │
│  (MCP config │    (singleton factory │   (CLI display:           │
│   injection) │     wires executor)   │    "Clicked (320,480)")   │
├──────────────┴───────────────────────┴───────────────────────────┤
│                      executor.ts (658 lines)                     │
│  createCliExecutor() → ComputerExecutor implementation           │
│  screenshot, click, type, drag, scroll, app management           │
├──────────────────┬───────────────────────────────────────────────┤
│  @ant/computer-  │  @ant/computer-use-swift                      │
│  use-input       │  (SCContentFilter screenshots,                │
│  (Rust/enigo:    │   NSWorkspace apps, TCC permissions,          │
│   mouse, kbd,    │   CGEventTap for ESC hotkey,                  │
│   frontmost app) │   CFRunLoop drain)                            │
├──────────────────┴───────────────────────────────────────────────┤
│                  drainRunLoop.ts                                  │
│  setInterval 1ms pumping CFRunLoop so Swift @MainActor           │
│  methods and Rust DispatchQueue.main calls resolve under libuv   │
└──────────────────────────────────────────────────────────────────┘
```

## Triple-Layer Feature Gating

### Layer 1: Compile-Time (`feature('CHICAGO_MCP')`)

Dead-code eliminates all CU imports in builds without the flag. All dynamic imports are wrapped:
```typescript
if (feature('CHICAGO_MCP') && getPlatform() === 'macos' && !getIsNonInteractiveSession()) {
  const { setupComputerUseMCP } = await import('./utils/computerUse/setup.js')
  // merge mcpConfig and allowedTools
}
```

### Layer 2: Runtime Config (`tengu_malort_pedway`)

GrowthBook remote config controlling `enabled` plus all sub-gates:
```typescript
const DEFAULTS: ChicagoConfig = {
  enabled: false,              // master switch
  pixelValidation: false,      // click verification (skipped in CLI)
  clipboardPasteMultiline: true,
  mouseAnimation: true,        // ease-out-cubic drag animation
  hideBeforeAction: true,      // hide non-target apps before action
  autoTargetDisplay: true,
  clipboardGuard: true,        // save/restore clipboard
  coordinateMode: 'pixels',    // frozen at first read
}
```

### Layer 3: Subscription Tier

```typescript
function hasRequiredSubscription(): boolean {
  if (process.env.USER_TYPE === 'ant') return true  // Anthropic employees
  const tier = getSubscriptionType()
  return tier === 'max' || tier === 'pro'
}
```

Additional guards: macOS only, interactive session only, monorepo override for ants.

## Native Modules

### `@ant/computer-use-input` (Rust/enigo)

Loaded lazily via `requireComputerUseInput()`. Provides:
- `moveMouse(x, y, animate)` -- instant or animated cursor movement
- `mouseButton(button, action, count)` -- click/press/release, left/right/middle
- `mouseScroll(amount, direction)` -- vertical/horizontal scroll
- `mouseLocation()` -- current cursor position
- `keys(parts)` -- xdotool-style key combos ("ctrl+shift+a")
- `key(name, action)` -- single key press/release
- `typeText(text)` -- grapheme-by-grapheme typing
- `getFrontmostAppInfo()` -- bundle ID + app name

### `@ant/computer-use-swift` (Swift)

Loaded at factory time. Provides:
- `screenshot.captureExcluding(allowList, quality, w, h, displayId)` -- SCContentFilter capture
- `screenshot.captureRegion(...)` -- zoomed region capture
- `display.getSize(displayId)` -- logical dims + scale factor
- `display.listAll()` -- multi-monitor enumeration
- `apps.prepareDisplay(allowList, host, displayId)` -- hide non-target apps
- `apps.listInstalled()` -- Spotlight-based app enumeration
- `apps.listRunning()` -- running app list
- `apps.open(bundleId)` -- launch app
- `apps.unhide(bundleIds)` -- restore hidden apps
- `apps.appUnderPoint(x, y)` -- hit-test
- `tcc.checkAccessibility()` -- macOS TCC permission check
- `tcc.checkScreenRecording()` -- macOS TCC permission check
- `hotkey.registerEscape(callback)` -- CGEventTap for ESC abort
- `_drainMainRunLoop()` -- pump CFRunLoop for libuv compatibility

## The CFRunLoop Problem

Under Electron, `DispatchQueue.main` drains automatically via CFRunLoop. Under Node.js/Bun (libuv), it never drains -- Swift `@MainActor` methods and Rust `dispatch2::run_on_main` calls hang forever.

Solution: `drainRunLoop.ts` runs a refcounted `setInterval` every 1ms calling `_drainMainRunLoop()`:

```typescript
let pump: ReturnType<typeof setInterval> | undefined
let pending = 0

export async function drainRunLoop<T>(fn: () => Promise<T>): Promise<T> {
  retain()  // start pump if not running
  try {
    const work = fn()
    work.catch(() => {})  // swallow orphan rejections on timeout
    const timeout = withResolvers<never>()
    timer = setTimeout(timeoutReject, 30_000, timeout.reject)
    return await Promise.race([work, timeout.promise])
  } finally {
    clearTimeout(timer)
    release()  // stop pump if no more pending calls
  }
}
```

## Tool Catalog (23 Tools)

| Tool | Category | Input | CLI Display |
|------|----------|-------|-------------|
| `request_access` | Permission | `{apps: [{displayName}]}` | app names |
| `list_granted_applications` | Permission | (none) | (hidden) |
| `screenshot` | Capture | `{displayId?}` | "Captured" |
| `zoom` | Capture | `{region: [x,y,w,h]}` | region coords |
| `left_click` | Mouse | `{coordinate: [x,y]}` | "(320, 480)" |
| `right_click` | Mouse | `{coordinate}` | "(x, y)" |
| `middle_click` | Mouse | `{coordinate}` | "(x, y)" |
| `double_click` | Mouse | `{coordinate}` | "(x, y)" |
| `triple_click` | Mouse | `{coordinate}` | "(x, y)" |
| `mouse_move` | Mouse | `{coordinate}` | "(x, y)" |
| `left_click_drag` | Mouse | `{coordinate, start?}` | "from -> to" |
| `left_mouse_down` | Mouse | (none) | (hidden) |
| `left_mouse_up` | Mouse | (none) | (hidden) |
| `cursor_position` | Mouse | (none) | (hidden) |
| `scroll` | Mouse | `{coordinate, direction, amount}` | "down x3" |
| `type` | Keyboard | `{text}` | "\"hello\"" |
| `key` | Keyboard | `{text}` | "ctrl+c" |
| `hold_key` | Keyboard | `{text, duration}` | key name |
| `wait` | Utility | `{duration}` | "5s" |
| `open_application` | App | `{bundle_id}` | bundle ID |
| `read_clipboard` | Clipboard | (none) | (hidden) |
| `write_clipboard` | Clipboard | `{text}` | "\"text\"" |
| `computer_batch` | Batch | `{actions: [...]}` | "N actions" |

## Screenshot Pipeline

```typescript
const SCREENSHOT_JPEG_QUALITY = 0.75

async screenshot(opts) {
  const d = cu.display.getSize(opts.displayId)
  // Pre-compute target dims so API backend skips resize
  const [targetW, targetH] = computeTargetDims(d.width, d.height, d.scaleFactor)
  return drainRunLoop(() =>
    cu.screenshot.captureExcluding(
      withoutTerminal(opts.allowedBundleIds),  // exclude our terminal
      SCREENSHOT_JPEG_QUALITY,
      targetW, targetH,
      opts.displayId,
    ),
  )
}
```

- Uses SCContentFilter (macOS Screen Recording API)
- Terminal window ALWAYS excluded from capture
- Pre-sizes to API target dimensions to avoid server-side resize
- JPEG quality 0.75

## Input Simulation

### Mouse Click Flow
```
moveAndSettle(x, y)           // instant move + 50ms HID round-trip wait
→ if modifiers:
    drainRunLoop(withModifiers(  // press/release bracket
      input.mouseButton(button, 'click', count)
    ))
  else:
    input.mouseButton(button, 'click', count)
```

### Keyboard Input
```
parts = keySequence.split('+')  // "ctrl+shift+a" → ["ctrl","shift","a"]
drainRunLoop(async () => {
  for (i = 0; i < repeat; i++) {
    if (i > 0) await sleep(8)   // 125Hz USB polling cadence
    if (isEsc) notifyExpectedEscape()  // punch hole in CGEventTap
    await input.keys(parts)
  }
})
```

### Clipboard Typing (Paste-via-Clipboard)
```
1. saved = pbpaste()           // save user's clipboard
2. pbcopy(text)                // write our text
3. VERIFY: pbpaste() === text  // round-trip check (never paste junk)
4. Cmd+V via keys()            // paste
5. sleep(100ms)                // wait for paste effect
6. finally: pbcopy(saved)     // restore user's clipboard
```

### Mouse Animation (Drag Only)
Ease-out-cubic at 60fps, distance-proportional duration at 2000 px/sec, capped 0.5s. Only used for drag-to motion (not regular moves).

## Security Architecture

### 1. App Allowlist
User must explicitly approve each app via `ComputerUseApproval.tsx` dialog. Two panels:
- TCC panel: checks Accessibility + Screen Recording macOS permissions
- App allowlist panel: checkboxes per app, sentinel warnings for shell/filesystem/system apps

### 2. ESC Abort Hotkey
CGEventTap consumes Escape system-wide during CU sessions:
- **Purpose**: Prompt-injection defense -- a PI-injected action cannot dismiss dialogs with Escape
- `notifyExpectedEscape()` punches a 100ms hole for model-synthesized Escapes
- If CGEventTap fails (missing Accessibility permission), CU still works without abort capability

### 3. Session Lock (`~/.claude/computer-use.lock`)
- O_EXCL atomic test-and-set (only one CU session across all Claude Code instances)
- Contains `{ sessionId, pid, acquiredAt }`
- Stale lock recovery via PID liveness check (`process.kill(pid, 0)`)
- Cleanup registered with shutdown handler

### 4. App Hiding
Before each action, non-target apps are hidden via `prepareDisplay()` so the model sees a clean workspace. Auto-unhidden at turn end (5-second timeout).

### 5. Terminal Exclusion
Terminal bundle ID detected via `__CFBundleIdentifier` env var (or fallback table for iTerm, Terminal.app, Ghostty, Kitty, Warp, VS Code). Terminal is stripped from screenshot allow-lists and never captured.

### 6. App Name Sanitization
Installed app names are sanitized before inclusion in tool descriptions:
- Char allowlist: `[\p{L}\p{M}\p{N}_ .&'()+-]` (no quotes, angle brackets, backticks)
- Max 40 chars per name, max 50 apps total
- Noise filtering: removes Helper, Agent, Service, Updater, Uninstaller apps
- Always-keep list: ~28 common apps (browsers, productivity, dev) bypass filters

### 7. OS Notifications
- Enter: "Claude is using your computer -- press Esc to stop"
- Exit: "Claude is done using your computer"

## Environment Variables

| Variable | Purpose |
|----------|---------|
| `ALLOW_ANT_COMPUTER_USE_MCP` | Override to enable CU for ants with monorepo |
| `COMPUTER_USE_INPUT_NODE_PATH` | Custom path to Rust .node binary |
| `COMPUTER_USE_SWIFT_NODE_PATH` | Custom path to Swift .node binary |
| `__CFBundleIdentifier` | macOS terminal detection |
| `USER_TYPE` | `'ant'` bypasses subscription check |

## Actionable Findings

1. **macOS only, cannot enable on Windows** -- Hard platform guard in `executor.ts` and `swiftLoader.ts`. Both native modules are darwin-only. No Linux or Windows implementation exists.

2. **Requires Max or Pro subscription** -- Even if we could run it, the subscription check would block us unless `USER_TYPE=ant`.

3. **The MCP naming is critical** -- Tools MUST be named `mcp__computer-use__*` because the API backend detects this prefix and injects a CU system prompt hint. This is why the MCP server name `computer-use` is reserved and cannot be used by user-configured servers.

4. **In-process, not subprocess** -- Despite being configured as a stdio MCP server, `client.ts` intercepts the connection by name and creates it in-process. The subprocess path (`--computer-use-mcp`) exists but is not the normal flow.

5. **The drainRunLoop pattern** -- If we ever need to call Swift/Rust native modules from Node.js that dispatch to DispatchQueue.main, this 1ms setInterval pump pattern is the solution Anthropic found.

6. **Coordinate mode is frozen** -- `coordinateMode` (pixels vs normalized) is locked at first read to prevent mid-session GrowthBook flips from desynchronizing the model's coordinate expectations with the executor's scaling.
