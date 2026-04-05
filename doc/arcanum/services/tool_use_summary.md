---
description: "tool use summary service — tool call result condensing, output summarization, context-efficient tool result display"
---

# Tool Use Summary Service -- Arcanum Wiki

## What Is This?

The Tool Use Summary service generates human-readable one-line labels describing what a batch of completed tool calls accomplished. These labels appear in the SDK's mobile app interface as single-line progress rows (truncated around 30 characters). Think git commit subjects, not sentences.

## How It Works

`generateToolUseSummary()` takes a list of completed tools (name, input, output) and:

1. Builds a concise text representation of each tool (name + truncated JSON input/output at 300 chars each)
2. Optionally includes the last assistant message text as context for the user's intent
3. Calls `queryHaiku()` -- a lightweight Haiku model call -- with a system prompt instructing past-tense, noun-focused labels
4. Returns the trimmed text response, or null on any error

The system prompt provides examples:
- "Searched in auth/"
- "Fixed NPE in UserService"
- "Created signup endpoint"
- "Read config.json"
- "Ran failing tests"

This is entirely non-critical -- errors are logged but never surface to the user.

## Key Source Files

| File | Purpose |
|------|---------|
| `toolUseSummaryGenerator.ts` | Summary generation via Haiku |

## Configuration

- No feature gates -- always available when the SDK requests it
- Uses the small/fast model (Haiku) for cost efficiency
- Prompt caching enabled for the system prompt

## Interesting Findings

1. **JSON truncation at 300 characters** keeps the prompt small. Tool outputs (which can be enormous file contents) are aggressively trimmed.

2. **The service is SDK-only** -- the CLI TUI does not use it. It exists to give mobile/web SDK consumers a progress indicator when tools are running.
