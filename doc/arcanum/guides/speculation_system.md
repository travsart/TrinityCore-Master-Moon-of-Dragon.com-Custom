---
description: "speculation system — predictive tool execution, copy-on-write overlay filesystem, pre-run Read during streaming, PromptSuggestion service"
---

# Guide: The Speculation System — Arcanum Wiki

> Claude Code's hidden predictive execution system that runs tool calls BEFORE the model asks for them.

## What Is Speculation?

One of the most fascinating discoveries in the source: Claude Code has a **speculative execution system** that predicts which tools the model will call and pre-runs them in the background. When the model actually requests the tool, the result is already ready — making Claude appear faster than should be physically possible.

This is analogous to CPU branch prediction but for AI tool calls.

## How It Works

```
Model is streaming a response...
  │
  ├── Speculation engine analyzes partial response
  │   "The model is about to call Read on config.ts..."
  │
  ├── Pre-executes Read("config.ts") in background
  │   → Result cached in overlay filesystem
  │
  ├── Model finishes streaming, requests: Read("config.ts")
  │   → Cache HIT — result returned instantly
  │   → User perceives zero tool latency
  │
  └── If prediction was wrong:
      → Cached result discarded
      → Normal tool execution proceeds
```

## The Overlay Filesystem

The speculation system uses a **copy-on-write overlay filesystem**:

- Speculative tool calls run against a virtual filesystem layer
- If a speculative Edit/Write modifies a file, the change lives ONLY in the overlay
- If the model confirms the action, the overlay change is committed to the real filesystem
- If the prediction was wrong, the overlay is discarded — no real files touched

This is critical for safety: speculative execution of Write/Edit tools must not actually modify files until confirmed.

**Source**: `services/PromptSuggestion/` — the Speculation system implementation.

## Permission Gating

Speculative tool calls still go through permission checks:
- Read-only tools (Read, Glob, Grep) are safe to speculate
- Write tools (Edit, Write) execute in the overlay only
- Dangerous tools (Bash) are NOT speculated
- Permission-denied tools are NOT speculated

The system errs heavily on the side of safety. A wrong speculation that reads a file is harmless. A wrong speculation that runs `rm -rf` would be catastrophic.

## Cache-Sharing Fork Pattern

The speculation engine uses the same fork pattern as subagents — it creates a lightweight fork that shares the parent's prompt cache. This means the prediction model's API call gets a massive cache hit on the existing conversation, making prediction cheap.

## Why This Matters for Power Users

1. **Perceived performance**: Claude Code feels faster than raw API latency would suggest. This is why.
2. **Tool order**: If you notice Claude's first tool call returns suspiciously fast, speculation probably pre-ran it.
3. **Read-heavy workflows benefit most**: The speculation system excels at predicting file reads. Write-heavy workflows see less benefit.
4. **Don't fight it**: If Claude seems to "know" what a file contains before reading it — it probably speculated the read during response streaming.

## Related: Prompt Suggestions

The speculation system is part of a broader "PromptSuggestion" service that also:
- Generates autocomplete suggestions for the prompt input
- Predicts what the user might type next
- Pre-loads relevant context based on predictions

The suggestion pipeline uses aggressive filtering (12 filter rules) to avoid showing bad predictions.

## Cross-References

- [API Overview](../api/api_overview.md) — request lifecycle
- [Tool Pipeline](../tools/pipeline_overview.md) — tool execution
- [Prompt Suggestions Service](../services/prompt_suggestions.md) — full service docs
