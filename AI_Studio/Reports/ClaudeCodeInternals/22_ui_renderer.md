# UI Renderer (Ink/React Terminal) — Claude Code Internals Report

> Report 22 | Generated 2026-04-05 | Source: v2.1.88 `src/` baseline + cli.js@2.1.97 grep refresh (2026-04-08)

## Overview

Claude Code's terminal UI is built on a heavily customized fork of [Ink](https://github.com/vadimdemedes/ink), the React-for-terminals framework. The system renders a full React component tree — including flexbox layout, streaming markdown, virtual scrolling, text selection, and mouse support — entirely within a terminal emulator using ANSI escape sequences. This is not a thin wrapper; Anthropic has rewritten Ink's internals to replace the upstream Yoga WASM binding with a pure-TypeScript layout engine, added a cell-level screen buffer with double-buffering and blit optimization, and implemented a sophisticated diff-based rendering pipeline that produces minimal terminal updates at ~60fps.

Since 2.1.89, Claude Code ships **two rendering modes**: an inline (default) mode where the Ink tree renders directly into the normal terminal scrollback and content scrolls naturally as it grows, and an opt-in **NO_FLICKER** mode enabled by `CLAUDE_CODE_NO_FLICKER=1` that wraps the entire React tree in an alt-screen compositor (`xj7` in the 2.1.97 bundle), claims the full terminal viewport, enables mouse tracking, and runs the cell-diff pipeline against the fixed-size alt-screen buffer. NO_FLICKER mode is auto-disabled under `tmux -CC` (iTerm2 integration) because that wrapper intercepts alt-screen sequences. About 56 distinct code paths branch on the `T4()` NO_FLICKER predicate — sticky footers, brief transcripts, paging keys, git-op counters, compact-boundary rendering, hyperlink click handlers, tool-result collapse heuristics, and more — so the modes are far from cosmetic: they produce materially different REPL behavior. Anthropic shipped ~15 NO_FLICKER bug fixes across 2.1.90–2.1.97. See Section 3b.

The rendering pipeline follows a clear path: React reconciler commits produce a virtual DOM of `ink-*` nodes, each with an associated Yoga layout node. On commit, the Yoga engine computes flexbox layout in a single pass. A renderer then walks the DOM tree, writing styled text into a cell-based `Screen` buffer. A `LogUpdate` diff engine compares the new screen against the previous frame, producing a minimal patch set (cursor moves, style transitions, text writes). These patches are optimized (merged, deduped) and serialized to ANSI escape sequences written to stdout. The entire pipeline runs in a single thread, throttled to 16ms frame intervals (~60fps), with double-buffering to avoid screen tearing. In inline mode (the default), the pipeline operates on the "live region" — the bottom-of-terminal area containing the current dirty content — and earlier content naturally scrolls up into terminal scrollback. This line-by-line streaming model (added in 2.1.78) is what allows normal text copy/paste in Claude Code's default mode. The changelog notes that line-by-line streaming was disabled on Windows in 2.1.81; the Ink stdin-drain helper (`wN_`) also early-returns on `process.platform === "win32"` before attempting the `/dev/tty` raw read loop, which means Windows has a materially different terminal-input path. NO_FLICKER mode bypasses the inline streaming model entirely and uses the cell-diff pipeline against the full alt-screen buffer.

The component hierarchy is deep: `main.tsx` creates the Ink root, wraps everything in `ThemeProvider`, then mounts `App` (which provides FPS metrics, stats, and app state contexts), `KeybindingSetup`, and finally the `REPL` screen component — a 5,005-line mega-component that manages the entire interactive session: message history, tool permissions, streaming text, prompt input, vim mode, transcripts, dialogs, and more.

## Architecture

### Key Files and Data Flow

```
main.tsx                    Entry point — creates Ink root via createRoot()
  ink.ts                    Public API — wraps all renders in ThemeProvider
    ink/root.ts             Root management (createRoot, render, unmount)
      ink/ink.tsx            Ink class — the rendering engine (~500 lines constructor + frame loop)
        ink/reconciler.ts    react-reconciler host config (creates DOM nodes, diffs props)
        ink/dom.ts           Virtual DOM (DOMElement, TextNode, markDirty)
        ink/renderer.ts      createRenderer() — DOM tree → Screen buffer
        ink/render-node-to-output.ts  Recursive tree walk, clipping, scrolling
        ink/output.ts        Operation collector (write/blit/clip/shift/clear → Screen)
        ink/log-update.ts    LogUpdate — Screen diff → Patch[]
        ink/optimizer.ts     Patch merge/dedupe
        ink/screen.ts        Cell-based screen buffer (TypedArray-backed)
        native-ts/yoga-layout/  Pure-TS flexbox engine (replaces WASM Yoga)

  screens/REPL.tsx           5,005-line mega-component (the entire chat UI)
  components/App.tsx         Top-level context providers (FPS, Stats, AppState)
  components/Markdown.tsx    Markdown → ANSI rendering with syntax highlighting
  components/TextInput.tsx   Text input with cursor, voice waveform, clipboard
  components/VimTextInput.tsx Vim-mode text input
  components/StatusLine.tsx  Bottom status bar
  components/VirtualMessageList.tsx  Virtual scrolling for message history
  dialogLaunchers.tsx        Dynamic-import dialog launchers
  interactiveHelpers.tsx     showDialog/showSetupDialog/renderAndRun utilities
```

### Rendering Pipeline (per frame)

1. **React commit** — reconciler calls `resetAfterCommit(rootNode)` after every React tree update
2. **Yoga layout** — `rootNode.onComputeLayout()` runs `calculateLayout()` with terminal width
3. **Schedule render** — `rootNode.onRender` triggers throttled `scheduleRender` (16ms interval)
4. **Render to screen** — `renderer()` walks DOM tree, produces `Screen` buffer via `Output` operations
5. **Diff** — `LogUpdate.render(prevFrame, nextFrame)` compares cell-by-cell, produces `Patch[]`
6. **Optimize** — merge consecutive cursor moves, concat style patches, dedupe hyperlinks
7. **Write** — `writeDiffToTerminal()` serializes patches to ANSI and writes to stdout
8. **Swap buffers** — front/back frame buffers swap (double-buffering)

### Dependencies

- `react` 19 + `react-reconciler` (ConcurrentRoot mode)
- `react/compiler-runtime` — React Compiler is enabled (all components use `_c()` cache slots)
- `marked` — Markdown lexing
- `@alcalzone/ansi-tokenize` — ANSI code parsing/diffing
- `chalk` — Color output
- `auto-bind` — Method binding in Ink class
- `lodash-es/throttle` — Render throttling
- `signal-exit` — Clean unmount on process exit

## Key Implementation Details

### 1. The React Reconciler (`src/ink/reconciler.ts`)

The reconciler is a `react-reconciler` host config targeting a custom virtual DOM. Element types are limited to: `ink-root`, `ink-box`, `ink-text`, `ink-virtual-text`, `ink-link`, `ink-progress`, `ink-raw-ansi`.

```typescript
// reconciler.ts:331-358 — createInstance
createInstance(originalType, newProps, _root, hostContext, internalHandle) {
  if (hostContext.isInsideText && originalType === 'ink-box') {
    throw new Error(`<Box> can't be nested inside <Text> component`)
  }
  const type = originalType === 'ink-text' && hostContext.isInsideText
    ? 'ink-virtual-text' : originalType
  const node = createNode(type)
  for (const [key, value] of Object.entries(newProps)) {
    applyProp(node, key, value)
  }
  return node
}
```

Key design choices:
- **React 19 compatibility** — `commitUpdate` receives old/new props directly (no updatePayload)
- **ConcurrentRoot** mode with `isPrimaryRenderer: true`
- **Event handler props** stored separately from attributes to avoid marking nodes dirty on handler identity changes (prevents defeating the blit optimization)
- **Debug repaints** — `CLAUDE_CODE_DEBUG_REPAINTS` env var enables owner-chain tracking on every node for attributing flicker sources
- **Commit instrumentation** — `CLAUDE_CODE_COMMIT_LOG` env var logs per-commit timing to a file

### 2. The Virtual DOM (`src/ink/dom.ts`)

Each `DOMElement` has:
- `yogaNode` — associated Yoga layout node (undefined for `ink-virtual-text`, `ink-link`, `ink-progress`)
- `dirty` flag — marks for re-rendering (propagated to ancestors via `markDirty()`)
- `scrollTop`, `pendingScrollDelta`, `stickyScroll` — scroll state for overflow containers
- `scrollAnchor` — one-shot anchor for `scrollToElement` (defers position read to paint time)
- `_eventHandlers` — click/hover handlers, separate from attributes
- `debugOwnerChain` — React component stack (only when `CLAUDE_CODE_DEBUG_REPAINTS` is set)

Text measurement uses `measureTextNode()` which calls `squashTextNodes()` to flatten nested text, expands tabs, measures with `measureText()`, and optionally wraps with `wrapText()`. The `ink-raw-ansi` type bypasses all of this — its producer pre-wraps content and declares exact dimensions via `rawWidth`/`rawHeight` attributes.

### 3. The Ink Engine (`src/ink/ink.tsx`)

The `Ink` class is the beating heart — ~500+ lines managing the entire rendering lifecycle:

- **Double-buffered frames** — `frontFrame` and `backFrame` with explicit pool management (`StylePool`, `CharPool`, `HyperlinkPool`)
- **Render throttling** — `scheduleRender = throttle(deferredRender, 16ms)` with both leading and trailing edges. The deferred render uses `queueMicrotask(this.onRender)` so layout effects (e.g., cursor declaration) commit before paint.
- **Alt-screen support** — toggled by `<AlternateScreen>`. When active: cursor position is clamped, mouse tracking is enabled, DECSTBM scroll optimization is available, selection overlay applies.
- **Text selection** — `SelectionState` tracks anchor/focus, supports word/line selection, scrolled-row capture, and OSC 52 clipboard integration.
- **Search highlighting** — position-based highlight scanning with current-index tracking.
- **Console patching** — intercepts `console.log/warn/error` and `stderr.write` to prevent mixing with Ink output.
- **SIGCONT handling** — re-enters alt screen, resets frame buffers, re-enables mouse tracking after terminal suspension.
- **Resize handling** — NOT debounced (debouncing creates a window where dimensions are inconsistent, causing double-blank flicker). Synchronous, with deferred erase-before-paint for atomic updates.

```typescript
// ink.tsx:213 — render scheduling
const deferredRender = (): void => queueMicrotask(this.onRender);
this.scheduleRender = throttle(deferredRender, FRAME_INTERVAL_MS, {
  leading: true, trailing: true
});
```

### 3b. NO_FLICKER Mode (opt-in alt-screen compositor, 2.1.89+)

Enabled by `CLAUDE_CODE_NO_FLICKER=1`. The predicate function (`T4()` in cli.js@2.1.97 around offset 7,055,000) returns `true` only when the env var is truthy and tmux -CC integration mode is NOT detected. When active, the REPL renders:

```js
if (T4()) return createElement(xj7, { mouseTracking: rc1() }, W_8);
return W_8;
```

where `xj7` (cli.js offset 12,471,461) is a React component that:

1. Writes `"\x1B[2J\x1B[H"` + the mouse-tracking enable sequence via `useInsertionEffect`, committing the escape before React's layout effects run.
2. Calls `setAltScreenActive(true, mouseTracking)` on the Ink instance (class `Rr6` at offset 4,093,260), which resets frame buffers for alt-screen mode.
3. Wraps children in a full-height Box: `<Box flexDirection="column" height={rows} width="100%" flexShrink={0}>`.
4. On unmount, calls `setAltScreenActive(false)`, `clearTextSelection()`, and writes the mouse-disable + alt-screen-exit sequences.

In inline mode (the default, `T4() === false`), the Ink engine still runs — same React reconciler, same Yoga layout, same Screen buffer, same LogUpdate diff pipeline — but content is rendered into the terminal's live region and older content scrolls up into terminal scrollback. Inline mode is what terminals can copy/paste naturally; NO_FLICKER is what produces a stable chrome-anchored UI similar to a full-screen TUI.

`CLAUDE_CODE_NO_FLICKER` is auto-disabled under tmux integration mode. The `Sg4()` probe runs `tmux display-message -p "#{client_control_mode}"`; when control mode is `"1"`, `T4()` returns false and a footer hint fires: `"fullscreen disabled: tmux -CC (iTerm2 integration mode) detected — set CLAUDE_CODE_NO_FLICKER=1 to override"`. Users can still force the mode under tmux -CC, but Anthropic warns it is unsupported.

**Gated behaviors** (non-exhaustive, collected from `T4()?...` branches in cli.js@2.1.97):

- **Sticky footer**: Permission dialogs (`psK`, `MsK`) receive a `setStickyFooter` callback only in NO_FLICKER mode. In inline mode, permission prompts scroll into place with the rest of the message stream and have no persistent footer affordance.
- **Brief transcript**: `ctrl+o` (transcript toggle) in NO_FLICKER mode toggles a `briefTranscript` overlay state on the current screen instead of swapping to a separate transcript screen. `ctrl+shift+b` is an explicit brief-transcript toggle (`app:toggleBrief`).
- **Compact boundary rendering**: `if (token.type === "system" && subtype === "compact_boundary") { if (T4()) return null; ... }` — compact boundary markers are hidden in NO_FLICKER (the alt-screen redraw handles the visual seam).
- **Git-op summary bar**: `bashCount`, `commits`, `pushes`, `branches`, `prs` counters are only initialized and rendered when `T4()` is true. Inline mode has no git-op summary.
- **Paging keys**: `pageUp`/`pageDown` map to scroll operations (`oQ8`) in NO_FLICKER; in inline mode they map to line-nav (`startOfLine`/`endOfLine`).
- **Hyperlink click handler**: `onHyperlinkClick` is attached to the Ink instance only in NO_FLICKER mode (mouse tracking is off in inline mode, so clicks don't reach the handler anyway).
- **Copy-on-select config item**: only visible in the settings menu when `T4()` is true.
- **Message list sticky prompt**: `scrollRef`, `trackStickyPrompt` are passed to `VirtualMessageList` only in NO_FLICKER mode.
- **Tool-result collapse heuristic**: `T4()` changes which tools collapse by default — bash tools are eligible for collapse in NO_FLICKER but kept expanded in inline mode.
- **Local JSX command rendering**: `isLocalJSXCommand` handling gated by `T4()`.
- **Message append behavior**: `aK` state updater uses `ikK` (NO_FLICKER ordered insert) vs. plain `[...t8, S8]` append in inline mode.

**Approximate scope**: ~56 distinct `T4()` call sites across ~38 unique 2KB regions of the bundle. Order-of-magnitude estimate: 500–1,000 LoC of NO_FLICKER-conditional logic, plus the shared alt-screen machinery in the Ink engine (`setAltScreenActive`, `resetFramesForAltScreen`, `altScreenActive`, `altScreenMouseTracking`, the SIGCONT re-entry path, the `reassertTerminalModes` loop).

**Historical note**: v2.1.88 had a `fullResetSequence_CAUSES_FLICKER` fallback inside `LogUpdate` for unreachable scrollback diffs. That symbol is **gone** in cli.js@2.1.97 (0 matches). The inline-rendering scroll path no longer needs this fallback because content that scrolls past the live region enters terminal scrollback and is never re-addressed — the diff engine's viewport-tracking logic short-circuits before the old full-reset path would have fired. NO_FLICKER alt-screen mode has its own reset via `resetFramesForAltScreen()` which is non-flicker (the alt-screen buffer is always fully addressable).

### 4. The Screen Buffer (`src/ink/screen.ts`, `src/ink/output.ts`)

The screen buffer is a cell-based representation backed by `Uint32Array` for performance. Each cell is packed into two 32-bit words encoding: character (via `CharPool` intern), style ID (via `StylePool` intern), cell width (narrow/wide/spacer), hyperlink ID (via `HyperlinkPool` intern), and a no-select flag.

`Output` collects operations during the tree walk:
- **write** — text with soft-wrap tracking
- **blit** — bulk cell copy from previous frame's screen (the O(unchanged) fast path)
- **clip/unclip** — nested overflow:hidden regions
- **shift** — DECSTBM hardware scroll simulation
- **clear** — damage marking for removed/shrunk nodes
- **noSelect** — marks regions excluded from text selection

The `charCache` in Output persists across frames (capped at 16,384 entries). For each unique line of text, it stores pre-computed `ClusteredChar[]` with grapheme clustering, style IDs, and hyperlink extraction — so the hot path is just property reads + `setCellAt`. ANSI tokenization and grapheme segmentation happen once per unique line.

### 5. The Diff Engine (`src/ink/log-update.ts`)

`LogUpdate.render()` is the diff core. It compares `prev.screen` and `next.screen` cell-by-cell using `diffEach()`, producing a `Diff` (array of `Patch` objects). Key behaviors:

- **Viewport tracking** — content that scrolled into scrollback is unreachable by cursor moves. If a diff touches scrollback rows, falls back to `fullResetSequence_CAUSES_FLICKER`.
- **Shrinking** — clears excess lines from bottom, adjusting cursor position.
- **Growing** — renders new rows directly (they naturally scroll the terminal via LF).
- **DECSTBM scroll optimization** — when a ScrollBox's scrollTop changes in alt-screen mode and the terminal supports synchronized output, uses hardware scroll regions (`CSI top;bot r` + `CSI n S/T`) instead of rewriting the entire region. `shiftRows()` on `prev.screen` simulates the shift so the diff loop only finds the newly-scrolled-in rows.
- **Width compensation** — for emoji that terminals may render as width-1 (old wcwidth tables), emits explicit `CHA` (Cursor Horizontal Absolute) sequences to force correct column positioning.
- **Style transitions** — uses `StylePool.transition(fromId, toId)` which returns cached pre-serialized ANSI strings. Zero allocations after warmup.

### 6. The Yoga Layout Engine (`src/native-ts/yoga-layout/index.ts`)

A pure-TypeScript port of Meta's Yoga (C++ flexbox engine). In v2.1.88 this pure-TS engine already coexisted with the upstream WASM Yoga binding (the WASM path was retained as a fallback); in **2.1.85** Anthropic removed the WASM fallback entirely, leaving the pure-TS engine as the sole layout backend. This was part of a scroll-performance push: scroll-heavy code paths now always hit the pure-TS engine, which has a single-slot layout cache (`_hasL`) and exposed performance counters (`yogaVisited`, `yogaMeasured`, `yogaCacheHits`, `yogaLive`). `yogaVisited` and `yogaCacheHits` symbols remain in cli.js@2.1.97. This is a complete single-pass flexbox implementation covering:
- flex-direction (row/column + reverse)
- flex-grow / flex-shrink / flex-basis
- align-items / align-self / align-content
- justify-content (all six values)
- margin / padding / border / gap
- width / height / min / max (point, percent, auto)
- position: relative / absolute
- display: flex / none / contents
- flex-wrap: wrap / wrap-reverse
- baseline alignment
- margin: auto
- Multi-pass flex clamping for min/max constraints

Not implemented (not used by Ink): aspect-ratio, content-box box-sizing, RTL direction.

The engine uses a single-slot layout cache (`_hasL`) to avoid redundant calculations. Performance counters (`yogaVisited`, `yogaMeasured`, `yogaCacheHits`, `yogaLive`) are exposed for frame-level profiling.

### 7. Markdown Rendering (`src/components/Markdown.tsx`)

Two components handle markdown:
- **`Markdown`** — for completed messages. Uses `marked.lexer()` with an LRU token cache (500 entries, keyed by content hash). Has a fast path: if no markdown syntax is detected (checked via regex on first 500 chars), skips the full GFM parse and creates a single paragraph token.
- **`StreamingMarkdown`** — for in-progress messages. Splits content at the last top-level block boundary: stable prefix (memoized, never re-parsed) + unstable suffix (re-parsed per delta). O(unstable length) per update instead of O(full text).

Syntax highlighting uses `cli-highlight` loaded via React Suspense (`use()` hook with `getCliHighlightPromise()`). First render shows unhighlighted markdown for ~50ms while the highlighter loads.

Tables are rendered as React components (`MarkdownTable`) with proper flexbox layout.

**Blockquotes** (added 2.1.97) are now also rendered as React components (`NRz` in the minified bundle, cli.js@2.1.97 offset 7,520,303), using a custom Ink border preset named `"quote"`:

```js
createElement(Box, {
  borderStyle: "quote",
  borderTop: false,
  borderBottom: false,
  borderRight: false,
  borderDimColor: true,
  paddingLeft: 1,
}, createElement(Text, { dimColor }, italicContent))
```

The `"quote"` border preset (cli.js offset ~4,069,662) is `{ top:" ", left:"▎", right:" ", bottom:" ", ... }` — only the left column is drawn, using U+258E (`▎`, "Left One Eighth Block"). Because top/bottom/right borders are disabled, the Box renders a **continuous left bar** down the full height of the quoted content, rather than the per-line `${bar} ${italic(line)}` string-prefix approach used in v2.1.88's `markdown.ts` (which split inner content on EOL and prefixed each line independently — liable to break across wraps). The legacy string-based `formatToken` path (`DP`) is still present as a fallback for non-React string-rendering contexts and still uses the split-and-prefix approach.

All other content goes through `formatToken()` which produces ANSI strings rendered via the `<Ansi>` component.

### 8. REPL.tsx — The Mega-Component (`src/screens/REPL.tsx`)

At 5,005 lines, REPL.tsx is the largest component. It manages:

- **Message history** — message list state, virtual scrolling, message selection (rewind)
- **Tool permissions** — `PermissionRequest` component dispatch, `ToolUseConfirm` callbacks
- **Streaming** — real-time text display during API responses
- **Prompt input** — text input with multiline, history, autocompletion, vim mode
- **Commands** — slash-command parsing and execution
- **Agent orchestration** — background tasks, local agents, swarm workers
- **Notifications** — terminal notifications, idle return dialog, cost threshold dialog
- **Transcript mode** — read-only view of conversation with interactive `/` search (2.1.83). In transcript mode (`screen === "transcript"`, virtual scroll active, search bar closed), a raw-key handler (`CK` in cli.js@2.1.97 offset 12,537,959, registered via `{ isActive: V6==="transcript" && DP6 && !Ql && !k6 }`) intercepts `/` to open an inline search bar overlay (`TqA` component, mounted in the `bottom` slot of the scroll scaffold): `if ($8 === "/") { aE.current?.setAnchor(); setSearchBarOpen(true); stopImmediatePropagation(); }`. A second handler processes `q` (exit), `[` (toggle past thinking), and `v` (render transcript to `$VISUAL`/`$EDITOR` via a temp `cc-transcript-<timestamp>.txt` file). While the search bar is open, `n` and `N` navigate matches via `aE.current.nextMatch` / `prevMatch`. **None of these bindings live in the formal Transcript keybinding context** (which only has `ctrl+e`, `ctrl+c`, `escape`, `q`) — they are raw-key handlers registered inside the REPL component directly.
- **Terminal title** — animated title showing agent status
- **Cost tracking** — per-turn and total cost display

Key imports reveal its scope: it imports from 80+ modules across tools, hooks, services, state management, and UI components.

### 9. Input Handling

**TextInput** (`src/components/TextInput.tsx`) wraps `BaseTextInput` with:
- Theme-aware cursor coloring
- Terminal focus detection (no cursor when unfocused)
- Accessibility mode (no inverse cursor when `CLAUDE_CODE_ACCESSIBILITY` is set)
- Voice recording waveform cursor (animated block character showing audio level)
- Clipboard image paste hint

**VimTextInput** (`src/components/VimTextInput.tsx`) wraps `BaseTextInput` with `useVimInput()` hook. The vim state machine (`src/vim/types.ts`) is a full implementation with:
- INSERT and NORMAL modes
- Operator-pending states (d/c/y + motion)
- Count prefix (1-9, max 10000)
- Find motions (f/F/t/T)
- Text objects (iw, aw, i", a(, etc.)
- Dot-repeat (recorded changes)
- Register (yank/paste)

As of **2.1.92**, there is **no standalone `/vim` slash command**. Vim mode is enabled exclusively through `/config` via the `editorMode` setting, whose options are `["normal", "vim"]`. Grep confirms `"/vim"` and `name:"vim"` produce 0 matches in cli.js@2.1.97, while the config-menu item and the `/config` help example `Enable vim mode: { "setting": "editorMode", "value": "vim" }` are present (offsets 9,392,025 and 10,141,101). `editorMode` is persisted in the global config (default `"normal"`); current vim sub-mode is exposed in SDK context under `"vim": { mode: "INSERT" | "NORMAL" }`, present only when vim mode is enabled (offset 4,924,628). A legacy `"emacs"` value is normalized to `"normal"` in the menu display code but is not in the options list.

**Keybindings** (`src/keybindings/`) — a layered context system:
- 16+ contexts: Global, Chat, Autocomplete, Settings, Confirmation, Tabs, Transcript, HistorySearch, Task, ThemePicker, Scroll, Help, Attachments, Footer, MessageSelector, DiffDialog, ModelPicker, Select, Plugin, MessageActions
- Default bindings in `defaultBindings.ts`, user overrides from `keybindings.json`
- **Key Global bindings** (verified in cli.js@2.1.97): `ctrl+c → app:interrupt`, `ctrl+d → app:exit`, `ctrl+t → app:toggleTodos`, `ctrl+o → app:toggleTranscript`, `ctrl+shift+b → app:toggleBrief`, `ctrl+shift+o → app:toggleTeammatePreview`, `ctrl+r → history:search`.
- **Transcript context bindings**: `ctrl+e → transcript:toggleShowAll`, `ctrl+c → transcript:exit`, `escape → transcript:exit`, `q → transcript:exit`. Note the transcript-mode `/` (search) and `n`/`N` (next/prev match) are handled as raw key events inside the REPL screen handler, not through the keybinding registry (see Section 8 "Transcript mode").
- Platform-specific keys (Windows: `alt+v` for image paste, `meta+m` for mode cycle when VT mode unavailable)
- Chord support (`ctrl+x ctrl+k` for kill agents, `ctrl+x ctrl+e` for external editor)

**Ctrl+O behavior varies by render mode**. In inline mode (default), `ctrl+o` swaps the REPL screen from `"prompt"` to `"transcript"` (a separate scroll-back view with its own key handlers and render path). In NO_FLICKER mode (Section 3b), `ctrl+o` toggles a `briefTranscript` overlay state on the current screen instead, avoiding a jarring screen swap. `ctrl+shift+b` is an explicit brief-transcript toggle.

### 10. Theme System (`src/components/design-system/ThemeProvider.tsx`)

- Three settings: `'dark'`, `'light'`, `'auto'`
- Auto mode probes terminal background color via OSC 11 (`systemThemeWatcher.js`)
- Theme context provides `currentTheme` (resolved, never 'auto') and `setThemeSetting`
- Preview support for ThemePicker (temporary theme without saving)
- All renders wrapped in `ThemeProvider` by `ink.ts`'s `withTheme()` helper
- Color values are raw (RGB, hex, ansi256, named ANSI) — theme resolution happens at component layer via `color('text', theme)` helper

### 11. Dialog/Modal System

Dialogs use two patterns:

1. **`showSetupDialog<T>(root, renderer)`** — renders into the Ink root with `AppStateProvider` + `KeybindingSetup` wrappers. Returns `Promise<T>` resolved by a `done` callback. Used for pre-REPL setup dialogs (settings validation, session chooser, teleport).

2. **`dialogLaunchers.tsx`** — thin async launchers that dynamically import dialog components. Each launcher wires `done` callbacks identically to the original inline call sites. Examples: `launchSnapshotUpdateDialog`, `launchInvalidSettingsDialog`, `launchAssistantSessionChooser`.

In-REPL dialogs (permissions, cost threshold, idle return, elicitation) are rendered as part of the REPL component tree, gated by state flags.

### 12. The Optimizer (`src/ink/optimizer.ts`)

Single-pass optimization on the `Diff` array before terminal write:
- Remove empty `stdout` patches
- Merge consecutive `cursorMove` patches (sum dx/dy)
- Collapse consecutive `cursorTo` (only last matters)
- Remove no-op cursor moves (0,0)
- Concat adjacent `styleStr` patches
- Dedupe consecutive hyperlinks with same URI
- Cancel cursor hide/show pairs
- Remove clear patches with count 0

### 13. Context-Low Footer Indicator (2.1.97 transient footer rewrite)

The context-window usage indicator (`vtK` in cli.js@2.1.97 offset 12,124,669) is a small React component rendered in the REPL footer region. It shows either `"${percent}% until auto-compact"` (normal) or `"Context low (${remaining}% remaining) · Run /compact to compact & continue"` (warning). In 2.1.97 the component returns `null` when context usage is below the warning threshold: `if (!isAboveWarningThreshold || j) return null;` (`j` is the verbose-status override). The element only mounts when the threshold is crossed, producing a **transient footer notification** that appears when context is low and disappears when compaction recovers headroom. Color transitions from `"warning"` to `"error"` via `isAboveErrorThreshold`. In "compact requested" / verbose modes, a dim variant with pre-suffix text is rendered instead.

## Configuration & Settings

### Environment Variables
| Variable | Purpose |
|---|---|
| `CLAUDE_CODE_NO_FLICKER` | Opt into alt-screen full-terminal rendering (flicker-free; disables inline streaming). Auto-disabled under tmux -CC (2.1.89+) |
| `CLAUDE_CODE_DEBUG_REPAINTS` | Enable component owner-chain tracking for flicker attribution |
| `CLAUDE_CODE_COMMIT_LOG` | Log per-commit timing to file (yoga, reconcile, paint durations) |
| `CLAUDE_CODE_DISABLE_TERMINAL_TITLE` | Disable animated terminal title |
| `CLAUDE_CODE_DISABLE_VIRTUAL_SCROLL` | Disable virtual scrolling for message list |
| `CLAUDE_CODE_DISABLE_MOUSE` | Disable mouse tracking in alt-screen (read via `rc1()` inside NO_FLICKER path) |
| `CLAUDE_CODE_DISABLE_MESSAGE_ACTIONS` | Disable message action bar |
| `CLAUDE_CODE_ACCESSIBILITY` | Disable inverse cursor (for screen readers) |
| `COLORFGBG` | Terminal foreground/background hint for auto-theme |
| `TERM_PROGRAM` | Terminal identification (used for xterm.js detection) |
| `NODE_ENV=development` | Enables React DevTools connection attempt |

### Frame Interval
```typescript
// ink/constants.ts
export const FRAME_INTERVAL_MS = 16  // ~60fps target
```

### Feature Flags (build-time via `bun:bundle`)
- `VOICE_MODE` — voice recording, waveform cursor, push-to-talk keybinding
- `MESSAGE_ACTIONS` — message action bar (shift+up, copy, paste)
- `QUICK_SEARCH` — global search dialog (ctrl+shift+f)
- `TERMINAL_PANEL` — terminal panel toggle (meta+j)
- `AUTO_THEME` — OSC 11 terminal theme detection
- `PROACTIVE` / `KAIROS` / `KAIROS_BRIEF` — proactive suggestions, brief mode
- `REVIEW_ARTIFACT` — review artifact tool + permission UI
- `WORKFLOW_SCRIPTS` — workflow tool + permission UI
- `MONITOR_TOOL` — monitor tool + permission UI
- `AGENT_TRIGGERS` — scheduled agent tasks

## Exploitation Opportunities

1. **Frame profiling** — Set `CLAUDE_CODE_COMMIT_LOG=/tmp/frames.log` to get per-frame timing breakdown (yoga layout, reconcile, paint, diff durations). Useful for diagnosing UI lag.

2. **Debug repaints** — Set `CLAUDE_CODE_DEBUG_REPAINTS=1` to track which React component causes each full-screen reset (flicker). The system logs `findOwnerChainAtRow()` results when `fullResetSequence_CAUSES_FLICKER` fires.

3. **Markdown performance** — The token cache (500 entries, LRU) and markdown-syntax-detection fast path mean short plain-text messages skip the ~3ms `marked.lexer()` call entirely. If building a similar system, this pattern is worth copying.

4. **Screen buffer design** — The `Output.charCache` pattern (caching pre-processed styled characters per unique line) turns the hot rendering loop into pure property reads. This is why Claude Code can render ~60fps even with thousands of lines of content.

5. **Blit optimization** — Clean subtrees (no dirty nodes) copy their cached screen region from the previous frame instead of re-rendering. This makes steady-state frames (spinner tick, clock update) touch only O(changed cells) instead of O(rows * cols).

6. **Virtual scrolling** — `VirtualMessageList.tsx` only mounts messages visible in the viewport, with Yoga layout nodes for unmounted messages preserving their height. This keeps the React tree shallow even for conversations with hundreds of turns.

## Edge Cases & Gotchas

1. **Pending wrap state** — When the cursor is at `x >= viewport_width`, the terminal enters "pending wrap" state. The diff engine must emit `\r` (carriage return) before cursor moves to resolve this without advancing to the next line. Multiple code paths handle this explicitly.

2. **Wide character compensation** — Emoji from Unicode 12.0+ and text-presentation emoji with VS16 (U+FE0F) may be rendered as width-1 by terminals with old wcwidth tables. The diff engine emits explicit `CHA` (cursor horizontal absolute) sequences after such characters to force correct positioning. A styled space is pre-written at x+1 to fill the gap on terminals where the emoji only advances 1 column.

3. **Alt-screen cursor clamping** — In alt-screen, `cursor.y` is clamped to `terminalRows - 1`. Without this, when content fills the screen exactly, the cursor-restore LF scrolls one row off the top of the alt buffer, desyncing the diff's cursor model.

4. **SIGCONT race** — After terminal suspension (ctrl+z), the shell may have written to the screen. The SIGCONT handler resets all frame buffers and the LogUpdate state. In alt-screen, it also re-enters the alternate buffer and re-enables mouse tracking.

5. **Scrollback unreachability** — Content that scrolled into terminal scrollback (above the viewport) cannot be reached by cursor movement. In v2.1.88 the diff engine had a `fullResetSequence_CAUSES_FLICKER` fallback to full-screen reset when a diff touched scrollback rows. That symbol is **gone** in cli.js@2.1.97 (0 matches): the inline-rendering viewport-tracking logic now short-circuits before the old fallback would fire (content past the live region simply enters scrollback and is never re-addressed). NO_FLICKER alt-screen mode has its own reset via `resetFramesForAltScreen()` which is non-flicker because the alt-screen buffer is always fully addressable.

6. **Synchronized output** — When the terminal supports DEC 2026 (BSU/ESU), DECSTBM scroll + diff write are wrapped in a synchronization block so the intermediate state (scrolled but not yet repainted) is never visible. Without it, the scroll optimization is disabled to avoid visual jumps.

7. **CharPool/HyperlinkPool reset** — HyperlinkPool resets every 5 minutes (to prevent unbounded growth). CharPool and screens are migrated across pool generations. StylePool is session-lived (never reset) because style IDs are cached in `Output.charCache`.

8. **React Compiler** — Every component uses `_c()` cache slots from `react/compiler-runtime`. This means the React Compiler is active, auto-memoizing components. Some components opt out with `'use no memo'` pragma (e.g., `StreamingMarkdown` which mutates refs during render by design).

## Cross-References

- **Report 06 (Tool Pipeline)** — Tool results flow through the UI as messages rendered by `Messages.tsx` → `MessageResponse.tsx` → `Markdown.tsx`
- **Report 10 (Permissions)** — `PermissionRequest.tsx` dispatches to tool-specific permission UIs, rendered inline in REPL
- **Report 21 (Feature Flags)** — `bun:bundle` feature() gates control which UI components are compiled in
- **Report 03 (Context Window)** — Token counts displayed in StatusLine, cost tracking in REPL
- **Report 11 (Skills)** — Slash commands parsed and executed within REPL's command handling
- **Report 07 (Swarm)** — Agent tasks, worker permissions, and team context managed by REPL state
