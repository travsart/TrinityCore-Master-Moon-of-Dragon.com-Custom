---
description: "UI layout — Yoga flexbox engine, terminal size handling, width calculation, responsive design, scroll management"
---

# Yoga Layout Engine -- Arcanum Wiki

## Overview

Claude Code uses Facebook's Yoga layout engine (a C++ implementation of CSS flexbox) to calculate component positions and sizes in the terminal. This enables complex layouts with flexible sizing, alignment, padding, borders, and scroll management -- capabilities far beyond traditional terminal line-by-line output.

## How It Works

### Layout Pipeline

1. React components declare layout properties via JSX (width, height, flexDirection, padding, etc.)
2. The custom reconciler translates these into Yoga node properties on the virtual DOM
3. `yogaNode.calculateLayout(terminalWidth, terminalHeight)` computes all positions
4. The renderer walks the DOM tree, reading computed positions to place content in the screen buffer

### Terminal Size Handling

Terminal dimensions are read via `stdout.columns` and `stdout.rows`. On resize (SIGWINCH), the layout is recalculated and a full redraw is triggered. The `prevFrameContaminated` flag is set to prevent differential rendering from producing artifacts.

### Key Layout Patterns

**Main layout**: A vertical flex container filling the terminal width. The prompt input is at the bottom with `flexShrink: 0`, while the conversation area above it takes remaining space with `flexGrow: 1`.

**Alt-screen layout**: `<AlternateScreen>` wraps children in a `<Box height={rows} flexShrink={0}>`, pinning the layout to exactly terminal height. The alt-screen buffer replaces the normal terminal output.

**Scroll management**: Content that overflows the viewport is managed by scroll state tracking. `consumeFollowScroll()` and `didLayoutShift()` handle auto-scrolling to follow new content while preserving manual scroll positions.

### Width Calculation

`get-max-width.ts` handles the complex task of calculating maximum width for text content, accounting for:
- East Asian wide characters (2 cells wide)
- Grapheme clusters (combined emoji, diacritical marks)
- ANSI escape sequences (zero visual width)
- Tab stops (configurable via `tabstops.ts`)
- Hyperlink escape sequences

The `stringWidth.ts` module provides accurate visual width measurement, critical for correct cursor positioning.

### Border Rendering

`render-border.ts` handles box-drawing characters for component borders. It supports different border styles and accounts for the border taking space in the layout.

### Performance Optimizations

- **Yoga counters**: `getYogaCounters()` tracks layout calculation costs for performance monitoring
- **Node cache**: `node-cache.ts` caches DOM node lookups to avoid repeated tree traversals
- **Line width cache**: `line-width-cache.ts` memoizes string width calculations for lines that have not changed
- **Layout shift detection**: `didLayoutShift()` detects when content above the viewport changes size, requiring scroll adjustment

## Key Source Files

| File | Purpose |
|------|---------|
| `src/ink/layout/` | Yoga layout integration |
| `src/ink/get-max-width.ts` | Maximum width calculation |
| `src/ink/stringWidth.ts` | Visual width measurement |
| `src/ink/render-border.ts` | Box-drawing border rendering |
| `src/ink/tabstops.ts` | Tab stop configuration |
| `src/native-ts/yoga-layout/` | Yoga bindings |

## Cross-References

- [UI Renderer](renderer.md) -- How layout feeds into rendering
- [UI Components](components.md) -- Components that use layout

## Interesting Findings

**Yoga is the same engine used by React Native.** Claude Code benefits from the same battle-tested flexbox implementation that powers mobile apps, adapted for terminal character cells.

**The yoga node dimension validation is defensive.** The renderer checks for NaN, Infinity, and negative dimensions before proceeding. These can occur when `calculateLayout()` has not been called yet or when the terminal is resized to zero dimensions.
