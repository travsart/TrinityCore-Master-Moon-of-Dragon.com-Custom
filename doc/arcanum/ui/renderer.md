---
description: "terminal renderer — React Ink rendering pipeline, Yoga layout, component lifecycle, state management, double-buffered screens"
---

# Terminal Rendering Pipeline -- Arcanum Wiki

## Overview

Claude Code uses a custom fork of the Ink framework (React for terminals) to render its UI. The rendering pipeline converts a React component tree into terminal output through Yoga layout calculations, a screen buffer abstraction, and differential rendering that minimizes terminal writes. The system supports both normal mode (inline rendering above the prompt) and alt-screen mode (full-screen takeover).

## How It Works

### The Ink Instance

The `Ink` class in `src/ink/ink.tsx` is the core runtime. It manages:

- A React `FiberRoot` via a custom reconciler (based on `react-reconciler`)
- A Yoga layout engine for flexbox-based terminal layout
- A double-buffered screen system (front buffer + back buffer)
- Keyboard event dispatching, mouse tracking, and focus management
- Frame scheduling with throttled rendering

Key imports reveal the scope: `reconciler.ts` (React reconciler), `dom.ts` (virtual DOM), `screen.ts` (character cell buffers with pools), `selection.ts` (text selection), `terminal.ts` (TTY abstraction), and `termio/` (CSI/DEC/OSC escape sequences).

### Rendering Flow

```
React setState / external trigger
  -> markDirty(node)
  -> scheduleRender()
  -> render() [throttled at FRAME_INTERVAL_MS]
     -> reconciler.flushSync() [commit React updates to virtual DOM]
     -> node.yogaNode.calculateLayout() [Yoga flexbox layout]
     -> createRenderer(node)(options) [convert DOM to screen buffer]
        -> renderNodeToOutput() [walk DOM tree, write to Output]
        -> screen.build() [finalize character cells]
     -> optimize(backFrame) [merge unchanged regions]
     -> writeDiffToTerminal() [emit only changed characters]
```

### The Renderer

`createRenderer()` in `src/ink/renderer.ts` creates a closure that reuses an `Output` instance across frames (so the `charCache` persists for tokenization and grapheme clustering). Each call:

1. Validates Yoga dimensions (NaN/Infinity/negative checks)
2. Creates a screen buffer at the computed width/height
3. Calls `renderNodeToOutput()` to walk the DOM tree and populate the buffer
4. Returns a `Frame` with the screen, viewport dimensions, and cursor position

Alt-screen mode clamps height to `terminalRows` to enforce the invariant that overflow writes are silently dropped rather than corrupting terminal state.

### Screen Buffer Architecture

The screen buffer (`src/ink/screen.ts`) uses pooled character cells:

- `CharPool`: Reusable string interning for cell content
- `HyperlinkPool`: Reusable hyperlink URL storage
- `StylePool`: Reusable ANSI style combinations
- `CellWidth`: Tracks East Asian wide characters for correct cursor positioning

Cells store: character, foreground color, background color, bold/italic/underline/strikethrough flags, hyperlink reference, and width.

### Differential Output

`writeDiffToTerminal()` in `src/ink/terminal.ts` compares the front buffer (previous frame) with the back buffer (current frame) and emits only the differences. The `LogUpdate` class handles cursor positioning and scrolling for inline mode, while alt-screen mode uses `CURSOR_HOME` positioning.

The system detects frame contamination (`prevFrameContaminated` flag) for cases where the previous buffer was mutated post-render (selection overlays, alt-screen enter/resize, force redraw). When contaminated, blitting is skipped and a full redraw is performed.

### Optimization

The `optimize()` function in `src/ink/optimizer.ts` identifies unchanged screen regions between frames, avoiding redundant terminal writes. This is especially effective for static UI elements (headers, borders) that do not change between turns.

### Key UI Patterns

- **Throttled rendering**: Frames are scheduled at `FRAME_INTERVAL_MS` intervals to prevent excessive redraws during rapid state changes
- **Auto-bind**: The `Ink` class uses `auto-bind` to bind all methods, simplifying event handler registration
- **Signal handling**: `onExit` from `signal-exit` handles cleanup on SIGINT/SIGTERM
- **Console patching**: Optional console method replacement to route `console.log` through the Ink rendering pipeline

## Key Source Files

| File | Purpose |
|------|---------|
| `src/ink/ink.tsx` | Core Ink runtime, frame scheduling, keyboard dispatch |
| `src/ink/renderer.ts` | DOM-to-screen-buffer conversion |
| `src/ink/reconciler.ts` | Custom React reconciler for terminal DOM |
| `src/ink/screen.ts` | Character cell buffers with object pools |
| `src/ink/dom.ts` | Virtual DOM implementation |
| `src/ink/output.ts` | Text rendering with grapheme clustering |
| `src/ink/log-update.ts` | Differential terminal output for inline mode |
| `src/ink/terminal.ts` | TTY abstraction, diff writing |
| `src/ink/optimizer.ts` | Frame-to-frame optimization |
| `src/ink/layout/` | Yoga layout integration |

## Configuration

The rendering pipeline is not directly user-configurable. Terminal capabilities are auto-detected. The `--debug` flag enables layout dimension logging.

## Cross-References

- [UI Components](components.md) -- Key React components
- [UI Layout](layout.md) -- Yoga layout engine details

## Interesting Findings

**Double-buffered rendering.** The front/back frame pattern enables atomic screen updates -- the current frame is never partially written. The swap happens after the full diff is computed.

**Object pools reduce GC pressure.** `CharPool`, `HyperlinkPool`, and `StylePool` are generational -- pools may be replaced between frames, but within a frame, all cells share the same pool instances. The `migrateScreenPools()` function handles inter-generational transitions.

**Alt-screen height clamping prevents corruption.** A specific bug was found where `MessageSelector` rendered as a sibling of the `<FullscreenLayout>` Box, causing `yogaHeight > terminalRows`. The fix clamps height in alt-screen mode so overflow writes are silently dropped rather than desyncing virtual/physical cursors.
