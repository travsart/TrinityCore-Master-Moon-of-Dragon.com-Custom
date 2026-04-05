---
description: "moreright — unknown internal feature, investigation needed, potentially related to permission escalation or capability expansion"
---

# Moreright -- Arcanum Wiki

## What Is This?

Moreright is an internal-only feature whose real implementation is completely hidden from external builds. The external source contains only a stub file (`useMoreRight.tsx`) that is a no-op. The file header explicitly states: "Stub for external builds -- the real hook is internal only."

Based on the hook's type signature, Moreright is a React hook that intercepts the query pipeline at two points: before a query is sent (`onBeforeQuery`) and after a turn completes (`onTurnComplete`). It also has access to the message history, input value, and a way to render custom JSX.

## How It Works

The stub reveals the interface contract:

```typescript
export function useMoreRight(_args: {
  enabled: boolean;
  setMessages: (action: M[] | ((prev: M[]) => M[])) => void;
  inputValue: string;
  setInputValue: (s: string) => void;
  setToolJSX: (args: M) => void;
}): {
  onBeforeQuery: (input: string, all: M[], n: number) => Promise<boolean>;
  onTurnComplete: (all: M[], aborted: boolean) => Promise<void>;
  render: () => null;
}
```

The stub returns pass-through values:
- `onBeforeQuery` always returns `true` (allow the query to proceed)
- `onTurnComplete` is a no-op
- `render` returns `null`

The file is self-contained with no relative imports, because "Typecheck sees this file at `scripts/external-stubs/src/moreright/` before overlay, where `../types/` would resolve to `scripts/external-stubs/src/types/` (doesn't exist)."

## Feature Gating

- Internal builds only -- the real implementation is overlaid onto the stub during internal builds
- The `enabled` parameter suggests it can be toggled at runtime
- No GrowthBook flag visible in the stub

## User-Facing Behavior

Unknown for external users -- the feature is completely stubbed out. Based on the hook interface, the real implementation likely:
- Can intercept and potentially modify or block queries before they are sent
- Can react to completed turns (possibly for analytics, learning, or follow-up actions)
- Can inject custom UI via `setToolJSX` and `render()`
- Has access to modify the input value and message history

## Key Source Files

| File | Purpose |
|------|---------|
| `src/moreright/useMoreRight.tsx` | External stub (25 lines) |

## Configuration

None visible -- entirely internal.

## Interesting Findings

1. **The name "Moreright"** is opaque and likely an internal codename. It does not correspond to any public Claude Code feature.

2. **The overlay build system** is revealed: Anthropic maintains `scripts/external-stubs/` with stub files that get overlaid with real implementations for internal builds. This is how they keep internal features out of the open-source release.

3. **The `onBeforeQuery` returning a boolean** is significant -- it can block queries from being sent. This suggests Moreright might be a content filter, policy enforcement layer, or pre-processing step that can reject or modify user input before it reaches the model.
